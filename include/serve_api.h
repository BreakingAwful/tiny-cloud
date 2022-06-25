/*
 * serve_api.h - APIs for A Cloud
 */

#ifndef __SERVE_API_H__
#define __SERVE_API_H__

#include "csapp.h"
#include "json-c/json.h"

void serve_json(int fd, json_object *root);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
		char *longmsg);
void read_requesthdrs(rio_t *rp, int *content_length, char *token);
void serve_user(int fd, char *token);
void serve_login(int fd, char *cgiargs);
void serve_file(int fd, char *api, char *token);
void serve_create_file(int fd, char *api, char *token);
void serve_delete_files(int fd, rio_t *rp, char *api, char *token,
		int content_length);
void do_serve(int fd, rio_t *rp, char *api, char *cgiargs, char *token,
		int content_length);

#endif	// __SERVE_API_H__
