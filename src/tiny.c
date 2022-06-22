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
void read_requesthdrs(rio_t *rp, int *content_length);
int parse_uri(char *uri, char *filename, char *cgiargs);
int simplify_uri(char *uri);
void serve_static(int fd, char *filename, size_t filesize,
                  char *request_method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *request_method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void serve_json(int fd, char *json);
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
  int is_static, content_length = 0;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // Read request line and headers
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD") &&
      strcasecmp(method, "POST")) {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio, &content_length);  // process remaining headers

  // check and simplify the URI
  if (!simplify_uri(uri)) {
    clienterror(fd, uri, "400", "Bad Request",
                "Tiny does cannot parse this URI");
  }
  // printf("Simplified URI: %s\n", uri);

  // Parse URI from request
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {  // check if the file exists
    // clienterror(fd, filename, "200", "Not found",
    //             "Tiny couldn't find this file");
		serve_json(fd,
				"{\"data\": {\"token\": \"jax\", \"username\": \"Jax\"}, \"status\": 200}");
		return;
  }
  if (content_length != 0) {  // process request body for POST
    Rio_readnb(&rio, cgiargs, content_length);
    cgiargs[content_length] = '\0';
  }

  // Sleep(5);         // Test connection close
  if (is_static) {  // Serve static content
    if (!(S_ISREG(sbuf.st_mode) && (S_IRUSR & sbuf.st_mode))) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  } else {  // Serve dynamic content
    if (!(S_ISREG(sbuf.st_mode) && (S_IXUSR & sbuf.st_mode))) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  // Build the HTTP response body
  sprintf(body,
          "<html><title>Tiny Error</title>"
          "<body bgcolor="
          "ffffff"
          ">\r\n"
          "%s: %s\r\n"
          "<p>%s: %s\r\n"
          "<hr><em>The Tiny Web server</em>\r\n",
          errnum, shortmsg, cause, longmsg);

  // Print the HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %ld\r\n\r\n", strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void serve_json(int fd, char *json) {
  char buf[MAXLINE];
	size_t len = strlen(json);

	// Write response headers
	sprintf(buf,
			"HTTP/1.0 200 OK\r\n"
			"Content-type: application/json\r\n"
			"Content-length: %ld\r\n"
			"Connection: close\r\n\r\n",
			len);

  // Print the HTTP response
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, json, len);
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

// Simply read remaining request headers and print them
void read_requesthdrs(rio_t *rp, int *content_length) {
  char buf[MAXLINE], *p;

  do {
    Rio_readlineb(rp, buf, MAXLINE);
    if ((p = strstr(buf, "Content-Length:")) != NULL) {
      *content_length = strtol(p + 16, NULL, 10);
    }
    printf("%s", buf);
  } while (strcmp(buf, "\r\n"));
}

// Return 1 if static, 0 if dynamic
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (strstr(uri, "cgi-bin")) {  // Dynamic content
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    } else {
      strcpy(cgiargs, "");
    }
    sprintf(filename, ".%s", uri);
    return 0;
  } else {
    strcpy(cgiargs, "");
    sprintf(filename, ".%s", uri);
    if (filename[strlen(filename) - 1] == '/') {
      strcat(filename, "index.html");
    }
    return 1;
  }
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
    } else if (n == 2 && strncmp(lptr, "./", 2) == 0) {
      continue;  // Ignore this redundant "./"
    } else if (n == 3 && strncmp(lptr, "../", 3) == 0) {
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

void serve_dynamic(int fd, char *filename, char *cgiargs,
                   char *request_method) {
  char buf[MAXLINE], *emptylist[] = {NULL};
  size_t n;

  // Send first part of HTTP response headers
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  n = strlen(buf);
  if (rio_writen(fd, buf, n) != n && errno != EPIPE) {
    unix_error("rio_writen error");
  }
  sprintf(buf, "Server: Tiny Web Server\r\n");
  n = strlen(buf);
  if (rio_writen(fd, buf, n) != n && errno != EPIPE) {
    unix_error("rio_writen error");
  }

  if (Fork() == 0) {  // Child process
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", request_method, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
}
