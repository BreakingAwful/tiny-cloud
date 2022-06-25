/**
 * serve_api.c - APIs for A Cloud
 */

#include "serve_api.h"
#include "helper.h"

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

void serve_user(int fd, char *token) {
	printf("Log: token = %s\n", token);
	if (!strcmp(token, "")) {
		clienterror(fd, "/user", "401", "Unauthorized", "You should login first");
		return;
	}

	// TODO: check token

	json_object *root = json_object_new_object();
	json_object *data = json_object_new_object();
	json_object_object_add(data, "username", json_object_new_string("Jax"));
	json_object_object_add(data, "sex", json_object_new_string("男"));
	json_object_object_add(data, "describeWord", json_object_new_string("测试"));
	json_object_object_add(data, "photo", json_object_new_string("https://jaxvanyang.github.io/favicon.ico"));
	json_object_object_add(root, "data", data);
	serve_json(fd, root);
	json_object_put(root);
}

void serve_login(int fd, char *cgiargs) {
	char username[MAXLINE], password[MAXLINE];
	char *p, *q;
	size_t len;

	// Empty string
	*username = *password = '\0';

	if ((p = strstr(cgiargs, "username="))) {
		if ((q = index(p, '&'))) {
			len = q - p - 9;
			strncpy(username, p + 9, len);
			username[len] = '\0';
		} else {
			strcpy(username, p + 9);
		}
	}

	if ((p = strstr(cgiargs, "password="))) {
		if ((q = index(p, '&'))) {
			len = q - p - 9;
			strncpy(password, p + 9, len);
			password[len] = '\0';
		} else {
			strcpy(password, p + 9);
		}
	}

	printf("cgiargs = %s, username = %s, password = %s\n", cgiargs, username, password);

	// TODO: check user info

	json_object *root = json_object_new_object();
	json_object *data = json_object_new_object();
	json_object_object_add(data, "token", json_object_new_string(username));
	json_object_object_add(data, "username", json_object_new_string(username));
	json_object_object_add(data, "photo", json_object_new_string("https://jaxvanyang.github.io/favicon.ico"));
	json_object_object_add(root, "data", data);
	serve_json(fd, root);
	json_object_put(root);
}

void serve_file(int fd, char *api, char *token) {
	char path[MAXLINE], *relative_path;
	struct stat sbuf;
	DIR *dirp;
	json_object *root, *data, *children;

	if (strlen(token) == 0) {
		clienterror(fd, api, "400", "Bad Request", "Invalid token");
		return;
	}

	relative_path = api + 4;
	if (*relative_path == '\0') {
		strcpy(relative_path, "/");
	}
	sprintf(path, "file/%s%s", token, relative_path);

	if (stat(path, &sbuf) < 0) {
		clienterror(fd, api, "404", "Not Found",
				"File or directory does not exist");
		return;
	}

	if (S_ISREG(sbuf.st_mode)) {
		root = json_object_new_object();

		// Build root
		data = json_object_new_object();
		json_object_object_add(root, "data", data);

		// Build data entry
		children = json_object_new_array();
		json_object_object_add(data, "fileData", children);
		json_object_object_add(data, "location", json_object_new_string(relative_path));
		json_object_object_add(data, "locationFiles", json_object_new_array());
		// json_object_object_add(data, "path", json_object_new_string(relative_path));
		// json_object_object_add(data, "isFolder", json_object_new_boolean(0));
		// json_object_object_add(data, "url", json_object_new_string(path));

		// Build fileData entry
		json_object *child = json_object_new_object();
		json_object_object_add(data, "fileData", child);
		json_object_object_add(child, "name", json_object_new_string(relative_path));
		json_object_object_add(child, "lastUpdateDate", json_object_new_string("2022-6-24"));
		json_object_object_add(child, "size", json_object_new_int(sbuf.st_size));
		json_object_object_add(child, "isFolder", json_object_new_boolean(0));

		// Build data entry
		json_object_put(root);
		return;
	}

	if ((dirp = opendir(path)) == NULL) {
		clienterror(fd, api, "403", "Forbidden", "You don't have the access");
		return;
	}

	// Build root
	root = json_object_new_object();
	data = json_object_new_object();
	json_object_object_add(root, "data", data);

	// Build data
	children = json_object_new_array();
	json_object_object_add(data, "fileData", children);
	json_object_object_add(data, "location", json_object_new_string(relative_path));
	json_object_object_add(data, "locationFiles", json_object_new_array());
	// json_object_object_add(data, "path", json_object_new_string(relative_path));
	// json_object_object_add(data, "isFolder", json_object_new_boolean(1));
	// json_object_object_add(data, "children", children);

	// Build fileData
	struct dirent *direntp = NULL;
	while ((direntp = readdir(dirp))) {
		char *name = direntp->d_name;
		if (*name == '.') continue;

		json_object *child = json_object_new_object();
		json_object_object_add(child, "name", json_object_new_string(name));
		json_object_object_add(child, "lastUpdateDate", json_object_new_string("2022-06-22"));
		json_object_object_add(child, "size", json_object_new_int(1024));
		json_object_object_add(child, "isFolder", json_object_new_boolean(direntp->d_type == DT_DIR));
		json_object_array_add(children, child);
	}
	serve_json(fd, root);
	json_object_put(root);
}

void do_serve(int fd, rio_t *rp, char *api, char *cgiargs, char *token) {
	printf("api: %s, %ld\ncgiargs: %s, %ld\ntoken: %s, %ld\n",
			api, strlen(api), cgiargs, strlen(cgiargs), token, strlen(token));

	if (str_start_with(api, "user")) {
		serve_user(fd, token);
	} else if(str_start_with(api, "login")) {
		serve_login(fd, cgiargs);
	} else if(str_start_with(api, "file")) {
		serve_file(fd, api, token);
	} else {
		clienterror(fd, api, "400", "Bad Request", "Tiny does not support this API");
	}
}
