/**
 * serve_api.c - APIs for A Cloud
 */

#include "serve_api.h"

void serve_json(int fd, json_object *root) {
  char buf[MAXLINE], body[MAXBUF];

	// Add status code 200
	json_object_object_add(root, "status", json_object_new_int(200));

	// Build json body
	sprintf(body, "%s",
			json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY));

  // Write the HTTP response
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: application/json\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %ld\r\n\r\n", strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  // Build the HTTP response body
	json_object *root = json_object_new_object();
	json_object_object_add(root, "cause", json_object_new_string(cause));
	json_object_object_add(root, "status", json_object_new_int(atoi(errnum)));
	json_object_object_add(root, "shortmsg", json_object_new_string(shortmsg));
	json_object_object_add(root, "longmsg", json_object_new_string(longmsg));
	sprintf(body, "%s", json_object_to_json_string_ext(root,
				JSON_C_TO_STRING_PRETTY));
	json_object_put(root);

  // Write the HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: application/json\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %ld\r\n\r\n", strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp, int *content_length, char *token) {
  char buf[MAXLINE], *p;

  do {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    if ((p = strstr(buf, "Content-Length:")) != NULL) {
      *content_length = strtol(p + 16, NULL, 10);
    } else if ((p = strstr(buf, "token:")) != NULL) {
			// puts("find token");
			strcpy(token, p + 7);
			size_t len = strlen(token);
			if (len >= 2) token[len - 2] = '\0';
		}
  } while (strcmp(buf, "\r\n"));
}

// TODO: implement this
void serve_user(int fd, char *token) {
	printf("Log: token = %s\n", token);

	json_object *root = json_object_new_object();
	json_object *data = json_object_new_object();
	json_object_object_add(data, "username", json_object_new_string("Jax"));
	json_object_object_add(data, "usersex", json_object_new_string("男"));
	json_object_object_add(data, "describeWord", json_object_new_string("测试"));
	json_object_object_add(data, "photo", json_object_new_string("/assets/images/user/1.jpg"));
	json_object_object_add(root, "data", data);
	serve_json(fd, root);
	json_object_put(root);
}

// TODO: implement this
void serve_login(int fd, char *cgiargs) {
	printf("cgiargs = %s\n", cgiargs);

	json_object *root = json_object_new_object();
	json_object *data = json_object_new_object();
	json_object_object_add(data, "token",
			json_object_new_string("Test token"));
	json_object_object_add(root, "data", data);
	serve_json(fd, root);
	json_object_put(root);
}

void do_serve(int fd, rio_t *rp, char *api, char *cgiargs, char *token) {
	if (!strcmp(api, "user")) {
		serve_user(fd, token);
	} else if(!strcmp(api, "login")) {
		serve_login(fd, cgiargs);
	} else {
		clienterror(fd, api, "400", "Bad Request", "Tiny does not support this API");
	}
}
