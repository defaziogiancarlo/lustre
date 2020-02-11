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
	
	test_single("version");
	test_single("timeout");
	test_single("ldlm/namespaces/*/lru_size");
	return 0;
}

