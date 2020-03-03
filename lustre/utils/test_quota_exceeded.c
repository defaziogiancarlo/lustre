#include <lustre/lustreapi.h>

int main(int argc, char *argv[])
{
	uid_t uid = 28153;
	char *fsname = "/mnt/lustre";

	int rc = llapi_exceeded_quota(fsname, uid);

	printf("%d\n", rc);
	return 0;

}
