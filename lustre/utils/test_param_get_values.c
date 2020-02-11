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





#include <lustre/lustreapi.h>
#include <stdio.h>

void test_pattern(char* pattern)
{
	printf("pattern=%s\n", pattern);
	glob_t paths;
	int rc;
	rc = llapi_get_lustre_param_paths(pattern, &paths);
	if (rc) {
		printf("bad pattern: %s\n", pattern);
		return;
	}
	
	char* buf;
	size_t sizeloc;
	FILE* fp; 

	int i;
	for (i = 0; paths.gl_pathv[i]; ++i) {
		fp = open_memstream(&buf, &sizeloc);
		llapi_get_lustre_param_all(paths.gl_pathv[i], fp);	       
		fclose(fp);
		printf("path=%s\n",paths.gl_pathv[i]);
		printf("value=\n%s\n", buf);
	}
}


int main(int argc, char* argv[])
{
	
	test_pattern("version");
	test_pattern("timeout");
	test_pattern("ldlm/namespaces/*/lru_size");
	return 0;
}

