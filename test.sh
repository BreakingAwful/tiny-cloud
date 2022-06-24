#!/bin/bash

function test_api() {
	echo -e "API: $1\n" && curl "localhost:8181/cloudbox$1" && echo -e "\n"
}

test_api "/user"

test_api "/login?username=a&password=b"

test_api "/login"

test_api "/file/"

curl "localhost:8181/cloudbox/file" -H "token: jax"
