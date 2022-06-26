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

void read_requesthdrs(rio_t *rp, int *content_length, char *token,
		char *boundary) {
  char buf[MAXLINE], *p;

	// Set default boundary
	strcpy(boundary, "--");

  do {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
		if ((p = strstr(buf, "Content-Length:")) ||
				(p = strstr(buf, "content-length:"))) {
      *content_length = strtol(p + 16, NULL, 10);
    } else if ((p = strstr(buf, "token:")) != NULL) {
			// puts("find token");
			strcpy(token, p + 7);
			size_t len = strlen(token);
			if (len >= 2) token[len - 2] = '\0';
		} else if ((p = strstr(buf, "boundary="))) {
			strcat(boundary, p + 9);
		}
  } while (strcmp(buf, "\r\n"));
}

void serve_user(int fd, char *token) {
	char file_path[MAXLINE];
	struct stat sbuf;

	if (!strcmp(token, "")) {
		clienterror(fd, "/user", "401", "Unauthorized", "You should login first");
		return;
	}

	// check token
	sprintf(file_path, "file/.user/%s", token);
	if (stat(file_path, &sbuf) < 0) {
		clienterror(fd, "/user", "400", "Bad Request", "Invalid token");
		return;
	}

	json_object *root = json_object_new_object();
	json_object *data = json_object_new_object();
	json_object_object_add(data, "username", json_object_new_string(token));
	json_object_object_add(data, "sex", json_object_new_string("男"));
	json_object_object_add(data, "describeWord", json_object_new_string("测试"));
	json_object_object_add(data, "photo", json_object_new_string("https://jaxvanyang.github.io/favicon.ico"));
	json_object_object_add(root, "data", data);
	serve_json(fd, root);
	json_object_put(root);
}

void serve_login(int fd, char *cgiargs) {
	char *username, *password;
	char file_path[MAXLINE], str[MAXSTR];
	FILE *file;

	if ((username = str_after(cgiargs, "username=")) == NULL) {
		clienterror(fd, "/login", "400", "Bad Request", "Invalid request username");
		return;
	}

	if ((password = find_str_after(username, "password="))== NULL) {
		clienterror(fd, "/login", "400", "Bad Request",
				"Invalid request password");
		return;
	}

	// Split username and password
	*strstr(username, "&password=") = '\0';

	if (!decode_url(username) || !decode_url(password)) {
		clienterror(fd, "/login", "400", "Bad Request", "Invalid request params");
		return;
	}
	printf("username = %s, password = %s\n", username, password);

	// Check user info
	sprintf(file_path, "file/.user/%s", username);
	if ((file = fopen(file_path, "r")) == NULL) {
		clienterror(fd, "/login", "400", "Bad Request", "User not exists");
		return;
	}
	fscanf(file, "%s", str);
	printf("str = %s\n", str);
	Fclose(file);
	if (strcmp(str, password)) {
		clienterror(fd, "/login", "400", "Bad Request", "Password not correct");
		return;
	}

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
	if (!decode_url(path)) {
		clienterror(fd, api, "400", "Bad Request", "Invalid folder path");
		return;
	}

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

void serve_create_file(int fd, char *api, char *token) {
	char folder_path[MAXSTR], command[MAXLINE];
	char *relative_path = api + 12;

	sprintf(folder_path, "file/%s%s", token, relative_path);
	if (!decode_url(folder_path)) {
		clienterror(fd, api, "400", "Bad Request", "Invalid folder path");
		return;
	}
	sprintf(command, "mkdir %s", folder_path);

	if (system(command)) {
		clienterror(fd, api, "500", "Internal Error", "Create folder error");
	} else {
		clienterror(fd, api, "200", "OK", "Create folder success");
	}
}

void serve_delete_files(int fd, rio_t *rp, char *api, char *token,
		int content_length) {
	char *json_string, file_path[MAXSTR], command[MAXLINE];
	const char *name;
	char *relative_path = api + 11;
	json_object *root, *resp, *data, *fail;
	int n, success = 1;

	json_string = Malloc(content_length + 1);
	Rio_readnb(rp, json_string, content_length);
	json_string[content_length] = '\0';
	root = json_tokener_parse(json_string);

	resp = json_object_new_object();
	data = json_object_new_object();
	fail = json_object_new_array();
	// Link components
	json_object_object_add(resp, "data", data);
	json_object_object_add(data, "fail", fail);

	// Build fail
	n = json_object_array_length(root);
	for (int i = 0; i < n; ++i) {
		json_object *name_obj = json_object_array_get_idx(root, i);
		name = json_object_get_string(name_obj);
		sprintf(file_path, "file/%s%s%s", token, relative_path, name);
		sprintf(command, "rm -r '%s'", file_path);
		if (system(command)) {
			json_object_array_add(fail, json_object_new_string(name));
			success = 0;
		}
	}

	json_object_object_add(data, "success", json_object_new_boolean(success));
	serve_json(fd, resp);

	json_object_put(root);
	json_object_put(resp);
	free(json_string);
}

void serve_upload_file(int fd, rio_t *rp, char *api, char *token,
		int content_length, char *boundary) {
	char *relative_path, *p;
	char file_path[MAXLINE], buf[MAXLINE + 10], filename[MAXSTR];
	FILE *file;
	int nread, boundary_len, total = 0;

	// Retrieve filename
	*buf = '\0';
	do {
		total += Rio_readlineb(rp, buf, MAXLINE);
		if ((p = strstr(buf, "filename="))) {
			copy_str_until(filename, p + 10, '\"');
			printf("filename = %s\n", filename);
		}
	} while (strcmp(buf, "\r\n"));
	relative_path = api + 10;
	sprintf(file_path, "file/%s%s%s", token, relative_path, filename);
	if (!decode_url(file_path)) {
		clienterror(fd, api, "400", "Bad Request", "Parent folder not exists");
		return;
	}
	printf("save_file_path: %s\n", file_path);

	// Read & write data
	file = Fopen(file_path, "w");
	boundary_len = strlen(boundary);
	boundary[boundary_len - 2] = '\0';
	for (nread = Rio_readlineb(rp, buf, MAXLINE); strcmp(buf, "\r\n");
			nread = Rio_readlineb(rp, buf, MAXLINE)) {
		total += nread;
		if ((p = strstr(buf, boundary))) {
			*p = '\0';
			fprintf(file, "%s", buf);
			break;
		}
		fprintf(file, "%.*s", nread, buf);
	}
	if (!strcmp(buf, "\r\n")) {
		total += Rio_readlineb(rp, buf, MAXLINE);
	}

	// Check if read to end
	printf("content_length: %d, total: %d\n", content_length, total);
	printf("lastline: %s", buf);
	if (total == content_length || strstr(buf, boundary)) {
		clienterror(fd, api, "200", "OK", "Upload file success");
	} else {
		clienterror(fd, api, "500", "Internal Error", "Upload file error");
	}

	Fclose(file);
}

void serve_check_user_name(int fd, char *api) {
	struct stat sbuf;
	char folder_path[MAXLINE];

	char *username = strchr(api, '/');
	if (!decode_url(username)) {
		clienterror(fd, api, "400", "Bad Request", "Invalid request user name");
		return;
	}

	sprintf(folder_path, "file%s", username);
	if (stat(folder_path, &sbuf) < 0) {
		clienterror(fd, api, "404", "Not Found", "User not exists");
		return;
	}

	clienterror(fd, api, "200", "OK", "User exists");
}

void serve_register(int fd, char *cgiargs) {
	char *username, *password;
	char folder_path[MAXSTR], command[MAXLINE], file_path[MAXLINE];
	FILE *file;
	struct stat sbuf;

	if ((username = str_after(cgiargs, "username=")) == NULL) {
		clienterror(fd, "/register", "400", "Bad Request", "Invalid request username");
		return;
	}

	if ((password = find_str_after(username, "password="))== NULL) {
		clienterror(fd, "/register", "400", "Bad Request",
				"Invalid request password");
		return;
	}

	// Split username and password
	*strstr(username, "&password=") = '\0';
	printf("username = %s, password = %s\n", username, password);

	if (!decode_url(username) || !decode_url(password)) {
		clienterror(fd, "/register", "400", "Bad Request", "Invalid request params");
		return;
	}

	sprintf(folder_path, "file/%s", username);
	if (stat(folder_path, &sbuf) >= 0) {
		clienterror(fd, "/register", "400", "Bad Request", "User already exists");
		return;
	}

	sprintf(command, "mkdir '%s'", folder_path);
	if (system(command)) {
		clienterror(fd, "/register", "500", "Internal Error",
				"Failed to create a new user");
		return;
	}

	// Store password
	sprintf(file_path, "file/.user/%s", username);
	file = Fopen(file_path, "w");
	fprintf(file, "%s", password);
	Fclose(file);

	clienterror(fd, "/register", "200", "OK", "Create a user success");
}

void do_serve(int fd, rio_t *rp, char *api, char *cgiargs, char *token,
		int content_length, char *boundary) {
	printf("api: %s, %ld\ncgiargs: %s, %ld\ntoken: %s, %ld\ncontent_length: %d\n",
			api, strlen(api), cgiargs, strlen(cgiargs), token, strlen(token), content_length);
	printf("boundary: %s, %ld\n", boundary, strlen(boundary));

	if (str_start_with(api, "user")) {
		serve_user(fd, token);
	} else if (str_start_with(api, "login")) {
		serve_login(fd, cgiargs);
	} else if (str_start_with(api, "file")) {
		serve_file(fd, api, token);
	} else if (str_start_with(api, "createFolder/")) {
		serve_create_file(fd, api, token);
	} else if (str_start_with(api, "deleteFiles/")) {
		serve_delete_files(fd, rp, api, token, content_length);
	} else if (str_start_with(api, "uploadFile/")) {
		serve_upload_file(fd, rp, api, token, content_length, boundary);
	} else if (str_start_with(api, "checkUserName/")) {
		serve_check_user_name(fd, api);
	} else if (str_start_with(api, "register")) {
		serve_register(fd, cgiargs);
	} else {
		clienterror(fd, api, "400", "Bad Request", "Tiny does not support this API");
	}
}
