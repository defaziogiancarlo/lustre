#include <lustre/lustreapi.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>

/**
 *  Indicate if a user is exceeding a quota.
 *  Return 0 if user is not exceeding quota
 *  Return 1 if user is exceeding quota
 *  Return negative value for errors
 */
int llapi_exceeded_quota(const char *fsname, uid_t uid)
{
	itn rc = 0;

	// set up input struct
	struct if_quotactl qctl;
	memset(&qctl, 0, sizeof(qctl));

	qctl.qc_cmd = LUSTRE_Q_GETQUOTA;
	qctl.qc_type = USRQUOTA;
	qctl.qc_id = uid;
	qctl.qc_dqblk.dqb_valid = 1;

	rc = llapi_quotactl(fsname, &qctl);
	if (rc < 0) {
		return rc;
	}

	// get the important bit
	return (int)((qctl.qc_dqinfo.qui_flags >> 2) & 1);
}
