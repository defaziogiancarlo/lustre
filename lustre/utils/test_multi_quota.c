#include <lustre/lustreapi.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

/* called with file system name, then reads uids from 
 * stdin and prints quota status (1 for exceeding, 0 for not exceeding)
 * to stdout. Output as single byte integers, not as characters.
 */

int mq2(char *fsname)
{
	int rc = 0;
	struct if_quotactl qctl;
	char buf[32];
	
	while (1) {
		/* initialize qctl for each user */
		memset(&qctl, 0, sizeof(qctl));
		qctl.qc_cmd = LUSTRE_Q_GETQUOTA;
		qctl.qc_type = USRQUOTA;
		qctl.qc_dqblk.dqb_valid = 1;

		/* get the uid */
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			rc = -errno;
			break;
		}

		qctl.qc_id = (unsigned)atoi(buf);

		/* get the quota infomation put into qctl */
		rc = llapi_quotactl(fsname, &qctl);
		if (rc < 0) {
			break;
		}
		putchar((int)((qctl.qc_dqinfo.dqi_flags >> 2) & 1));
	}

        return rc;
} 

int mq2text(char *fsname)
{
	int rc = 0;
	struct if_quotactl qctl;
	char buf[32];
	int uid = 0;
	
	while (1) {
		/* initialize qctl for each user */
		memset(&qctl, 0, sizeof(qctl));
		qctl.qc_cmd = LUSTRE_Q_GETQUOTA;
		qctl.qc_type = USRQUOTA;
		qctl.qc_dqblk.dqb_valid = 1;

		/* get the uid */
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			rc = -errno;
			break;
		}

		uid = (unsigned)atoi(buf);
		qctl.qc_id = uid;

		/* get the quota infomation put into qctl */
		rc = llapi_quotactl(fsname, &qctl);
		if (rc < 0) {
			break;
		}
		printf("%u=%s\n", uid, 
		       ((qctl.qc_dqinfo.dqi_flags >> 2) & 1) ? "True" : "False" );

		//putchar((int)((qctl.qc_dqinfo.dqi_flags >> 2) & 1));
	}

        return rc;
} 




// expects fsname on commandline
int main(int argc, char *argv[])
{	
	return mq2text(argv[1]);
}
