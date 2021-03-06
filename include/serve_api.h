/*
 * serve_api.h - APIs for A Cloud
 */

#ifndef __SERVE_API_H__
#define __SERVE_API_H__

#include "csapp.h"
#include "json-c/json.h"

void get_filetype(char *filename, char *filetype);
void serve_json(int fd, json_object *root);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
		char *longmsg);
void read_requesthdrs(rio_t *rp, int *content_length, char *token,
		char *boundary);
void serve_user(int fd, char *token);
void serve_login(int fd, char *cgiargs);
void serve_get_files(int fd, char *api, char *token);
void serve_create_file(int fd, char *api, char *token);
void serve_delete_files(int fd, rio_t *rp, char *api, char *token,
		int content_length);
void serve_upload_file(int fd, rio_t *rp, char *api, char *token,
		int content_length, char *boundary);
void serve_check_user_name(int fd, char *api);
void serve_register(int fd, char *cgiargs);
void serve_file(int fd, char *api, char *token);
void do_serve(int fd, rio_t *rp, char *api, char *cgiargs, char *token,
		int content_length, char *boundary);

#endif	// __SERVE_API_H__
