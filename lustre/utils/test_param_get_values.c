/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.gnu.org/licenses/gpl-2.0.html
 *
 * GPL HEADER END
 */



#include <stddef.h>
#include <lustre/lustreapi.h>
/* #include <stdio.h> */
/* #include <errno.h> */


/* int test_static_buffer() */
/* { */
/*         char* pattern = "version"; */
/*         glob_t paths; */
/*         int rc = 0;; */

/*         /\* find the file that holds the Lustre version *\/ */
/*         rc = llapi_param_get_paths(pattern, &paths); */
/*         if (rc != 0) { */
/*                 printf("bad pattern: %s\n", pattern); */
/*                 return rc; */
/*         } */
/*         printf("The parameter \'%s\' is stored at \'%s\'\n", pattern, paths.gl_pathv[0]); */

/*         /\* get the size of the file *\/ */
/*         size_t file_size; */
/*         rc = llapi_param_get_value(paths.gl_pathv[0], NULL, &file_size); */
/*         if (rc != 0) { */
/*                 printf("failed to get size of %s\n", (paths.gl_pathv[0])); */
/* 		return rc; */
/* 	} */
/* 	printf("the current file size is %lu bytes\n", file_size); */

/*         /\* allocate a buffer and copy the file into it *\/ */
/*         char* buf = calloc(file_size, sizeof(char)); */
/*         rc = llapi_param_get_value(paths.gl_pathv[0], &buf, &file_size); */
/* 	if (rc != 0) { */
/* 		if (rc != -EOVERFLOW) */
/* 			printf("fail here\n"); */
/* 			return rc;    */
/* 		printf("not enough space in buf, buf should be at least %lu bytes", file_size); */
/* 		return rc; */
/* 	} */
/* 	printf("the value is: %s", buf); */
/* 	return rc; */
/* } */

/* // TODO this fails, a larger buffer is not reallocated */
/* int guess_buffer_size(const char* path) */
/* { */
/* 	/\* incorrectly assuming the file holds "2.12.4" *\/ */
/* 	size_t guess = 7; */
/* 	int rc = 0; */
/* 	char* buf; */
/* 	size_t file_size; */

/* 	buf = calloc(guess, sizeof(char)); */
/* 	file_size = guess; */
/* 	while (llapi_param_get_value(path, &buf, &file_size) != 0) { */
/* 		if (rc == -EOVERFLOW) { */
/* 			guess = guess > file_size ? guess : file_size; */
/* 			file_size = guess; */
/* 			buf = calloc(guess, sizeof(char));			 */
/* 		} */
/* 		return rc; */
/* 	} */

/* 	printf("the parameter at %s\n has value %s", path, buf); */
/* 	return rc; */
/* } */

/* int automatic_buffer_size(const char* path) */
/* { */
/* 	int rc = 0; */
/* 	char *buf = NULL; */
/* 	size_t file_size; */

/* 	rc = llapi_param_get_value(path, &buf, &file_size); */
/* 	if (rc != 0)  */
/* 		return rc; */

/* 	printf("the prameter at %s\nhas a length of %lu\n and a value of=\n%s", path, file_size, buf); */
/* 	return rc; */
/* } */



/* #include <lustre/lustreapi.h> */
/* #include <stdio.h> */

/* void test_pattern(char* pattern) */
/* { */
/* 	printf("pattern=%s\n", pattern); */
/* 	glob_t paths; */
/* 	int rc; */
/* 	rc = llapi_param_get_paths(pattern, &paths); */
/* 	if (rc) { */
/* 		printf("bad pattern: %s\n", pattern); */
/* 		return; */
/* 	} */
	
/* 	char* buf = NULL; */
/* 	size_t sizeloc; */


/* 	int i; */
/* 	for (i = 0; paths.gl_pathv[i]; ++i) { */
/* 		llapi_param_get_value(paths.gl_pathv[i], &buf, &sizeloc); */
/* 		printf("path=%s\n",paths.gl_pathv[i]); */
/* 		printf("value=\n%s\n", buf); */
/* 	} */
/* } */


int main(int argc, char* argv[])
{
	
//	test_pattern("version");
//	test_pattern("timeout");
//	test_pattern("ldlm/namespaces/*/lru_size");

	/* int rc = test_static_buffer(); */
	/* rc = guess_buffer_size("/sys/fs/lustre/version"); */
	/* rc = automatic_buffer_size("/sys/fs/lustre/version"); */

	/* printf("%d\n", rc); */

	return 0;
}

