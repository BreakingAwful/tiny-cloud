/**
 * @file tiny.c
 * @author Jax (jaxvanyang@gmail.com)
 * @brief A simple, iterative HTTP/1.0 Web server that uses the GET method to
 * serve API for A Cloud.
 * @version 0.1
 * @date 2022-05-29
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "csapp.h"
#include "serve_api.h"
#include "helper.h"

#define MAXTYPE 128  // Max file type length

typedef struct PathNode {
  char *path;
  struct PathNode *pre, *next;
} PathNode;
PathNode *create_node(char *path);
PathNode *insert_node(PathNode *pre, PathNode *node);
PathNode *insert_path(PathNode *pre, char *path);
void free_list(PathNode *listp);

void doit(int fd);
int parse_uri(char *uri, char *api, char *cgiargs);
int simplify_uri(char *uri);
void get_filetype(char *filename, char *filetype);
void serve_static(int fd, char *filename, size_t filesize,
		char *request_method);
void sigchld_handler(int sig);
void sigpipe_handler(int sig);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // Check command line args
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  Signal(SIGCHLD, sigchld_handler);  // Reap dead child processes
  // Ignore broken pipe errors for connection close
  Signal(SIGPIPE, sigpipe_handler);

  listenfd = Open_listenfd(argv[1]);
  printf("Running on http://localhost:%s\n", argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
}

// Create a new node, remember to free the node and its path
PathNode *create_node(char *path) {
  PathNode *node = calloc(1, sizeof(PathNode));
  if (path) {  // Make sure the given path is not NULL
    node->path = malloc(strlen(path) + 1);
    strcpy(node->path, path);
  }
  return node;
}

/*
 * Insert a new node into the linked list
 * Return the node if succeed, otherwise return NULL
 */
PathNode *insert_node(PathNode *pre, PathNode *node) {
  if (pre == NULL || node == NULL) {
    return node;
  }

  node->pre = pre;
  if ((node->next = pre->next)) {  // If pre->next is not NULL
    node->next->pre = node;
  }
  pre->next = node;
  return node;
}

/*
 * Insert a new path into the linked list
 * Return the new node if succeed, otherwise return NULL
 */
PathNode *insert_path(PathNode *pre, char *path) {
  PathNode *node;
  if (pre == NULL) {
    return NULL;
  }

  node = create_node(path);
  node->pre = pre;
  if ((node->next = pre->next)) {  // If pre has a next node
    node->next->pre = node;
  }
  pre->next = node;
  return node;
}

void free_list(PathNode *listp) {
  PathNode *next;
  while (listp) {
    next = listp->next;
    if (listp->path) {
      free(listp->path);
    }
    free(listp);
    listp = next;
  }
}

void doit(int fd) {
  int is_valid, content_length = 0;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char api[MAXLINE], cgiargs[MAXLINE], token[MAXLINE], boundary[MAXLINE];
  rio_t rio;
	struct stat sbuf;

	// Empty string
	*buf = *method = *uri = *version = *api = *cgiargs = *token = '\0';

  // Read request line and headers
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD") &&
      strcasecmp(method, "POST") && strcasecmp(method, "DELETE")) {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }
	*token = '\0';
  read_requesthdrs(&rio, &content_length, token, boundary);
	if (content_length > MAXFILE) {
		clienterror(fd, uri, "500", "Too Large Body",
				"Tiny cannot handle this large request body");
		return;
	}

  // check and simplify the URI
  // if (!simplify_uri(uri)) {
	// 	clienterror(fd, uri, "400", "Bad Request",
	// 			"Tiny cannot parse this URI");
	// 	return;
  // }
  // printf("Simplified URI: %s\n", uri);

  // Parse URI from request
  is_valid = parse_uri(uri, api, cgiargs);
	if (!is_valid) {
		if (stat(api, &sbuf) >= 0 && S_ISREG(sbuf.st_mode) &&
				(S_IRUSR & sbuf.st_mode)) {
			// Used for development
			serve_static(fd, api, sbuf.st_size, method);
		} else {
			clienterror(fd, uri, "404", "Invalid API",
					"Tiny does not implement this API");
		}
		return;
	}

  // if (content_length > 0) {  // process request body for POST
  //   Rio_readnb(&rio, cgiargs, content_length);
  // }

	do_serve(fd, &rio, api, cgiargs, token, content_length, boundary);
}

void sigchld_handler(int sig) {
  sigset_t block_all, prev_set;
  int olderrnor = errno;

  // Block all signals to guarantee reaping succeeded
  Sigfillset(&block_all);
  Sigprocmask(SIG_BLOCK, &block_all, &prev_set);

  while (waitpid(-1, NULL, WNOHANG | WUNTRACED) > 0)
    ;
  if (errno != ECHILD) {
    unix_error("sigchld_handler error");
  }

  // Restore previous environment
  Sigprocmask(SIG_SETMASK, &prev_set, NULL);
  errno = olderrnor;
}

void sigpipe_handler(int sig) {
  int olderrnor = errno;

  Sio_puts("SIGPIPE caught\n");

  errno = olderrnor;
}

// Return 1 if valid, 0 otherwise
// uri example: /cloudbox/login?username=a&passwd=a
int parse_uri(char *uri, char *api, char *cgiargs) {
  char *ptr;

	// Used for development
	if (str_start_with(uri, "/file")) {
		sprintf(api, "%s", uri + 1);
		return 0;	// file is not API
	}

	if (!str_start_with(uri, "/cloudbox/")) return 0;

	ptr = index(uri, '?');
	if (ptr) {
		strcpy(cgiargs, ptr + 1);
		*ptr = '\0';
		sprintf(api, "%s", uri + 10);
		*ptr = '?';
	} else {
		sprintf(api, "%s", uri + 10);
	}

	return 1;
}

// Remove redundant "../"" and "./" from the URI if the uri is valid
// Return 1 if the uri is valid, 0 otherwise
int simplify_uri(char *uri) {
  size_t len, n;
  char *lptr, *rptr;
  PathNode *head, *tail;

  if (uri == NULL || uri[0] != '/' || (len = strlen(uri) == 0)) {
    return 0;  // URI must start with '/'
  }

  // Build the path list
  head = create_node(NULL);       // Create a dummy node
  tail = insert_path(head, "/");  // Always point to the last path segment
  for (lptr = uri + 1; *lptr != '\0'; lptr = rptr + 1) {
    if ((rptr = index(lptr, '/')) == NULL) {  // Reach the last segment [lptr:]
      tail = insert_path(tail, lptr);
      break;
    }

    // Normal segment [lptr:rptr], not the last
    n = rptr - lptr + 1;  // Length of the segment
    if (n == 1 && *lptr == '/') {
      continue;  // Ignore this redundant "/"
    } else if (n == 2 && str_start_with(lptr, "./")) {
      continue;  // Ignore this redundant "./"
    } else if (n == 3 && str_start_with(lptr, "../")) {
      // Ignore this redundant "../" and remove the previous segment
      if (tail->pre == head) {  // No previous segment makes a invalid URI
        free_list(head);
        return 0;
      }
      tail = tail->pre;
      free_list(tail->next);
    } else {  // Normal segment, not redundant
      PathNode *node = create_node(NULL);
      node->path = malloc(sizeof(char) * (n + 1));
      strncpy(node->path, lptr, n);
      tail = insert_node(tail, node);
    }
  }

  // Write the simplified URI in place
  for (tail = head->next, lptr = uri; tail; tail = tail->next) {
    n = strlen(tail->path);
    strncpy(lptr, tail->path, n);
    lptr += n;
  }
  *lptr = '\0';  // Terminate the string

  // Free the path list
  free_list(head);

  return 1;
}

// Derive file type from filename
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".mp4")) {
    strcpy(filetype, "video/mp4");
  } else
    strcpy(filetype, "text/plain");
}

void serve_static(int fd, char *filename, size_t filesize,
		char *request_method) {
	int srcfd;
	size_t n;
	char *srcp, filetype[MAXTYPE], buf[MAXBUF];

	// Send response headers to client
	get_filetype(filename, filetype);
	sprintf(buf,
			"HTTP/1.0 200 OK\r\n"
			"Server: Tiny Web Server\r\n"
			"Connection: close\r\n"
			"Content-length: %ld\r\n"
			"Content-type: %s\r\n\r\n",
			filesize, filetype);
	// Rio_writen(fd, buf, strlen(buf));
	n = strlen(buf);
	if (rio_writen(fd, buf, n) != n) {
		if (errno == EPIPE) {  // Client closed connection
			return;
		} else {
			unix_error("rio_writen error");
		}
	}
	if (!strcasecmp(request_method, "HEAD")) return;
	printf("Response headers:\n%s", buf);

	// Send response body to client
	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	// Rio_writen(fd, srcp, filesize);
	if (rio_writen(fd, srcp, filesize) != filesize && errno != EPIPE) {
		unix_error("rio_writen error");
	}
	// Client may close too early here, but later operations are the same
	Munmap(srcp, filesize);
}
