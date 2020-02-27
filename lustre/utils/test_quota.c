/* get the quota data and look at it */

#include <lustre/lustreapi.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>


// stolen from lfs.c
static int name2uid(unsigned int *id, const char *name)
{
	struct passwd *passwd;

	passwd = getpwnam(name);
	if (passwd == NULL)
		return -ENOENT;
	*id = passwd->pw_uid;

	return 0;
}




int init_quota_args()
{
	return 0;
}


void print_obd_dqinfo(struct obd_dqinfo *x)
{
	printf("%s=%llu\n","dqi_bgrace", x->dqi_bgrace);
	printf("%s=%llu\n","dqi_igrace", x->dqi_igrace);
	printf("%s=%u\n","dqi_flags",   x->dqi_flags);
	printf("%s=%u\n","dqi_valid",   x->dqi_valid);
}

void print_obd_dqblk(struct obd_dqblk *x)
{
	printf("%s=%llu\n","dqb_bhardlimit", x->dqb_bhardlimit);
	printf("%s=%llu\n","dqb_bsoftlimit", x->dqb_bsoftlimit);
	printf("%s=%llu\n","dqb_curspace",   x->dqb_curspace);
	printf("%s=%llu\n","dqb_ihardlimit", x->dqb_ihardlimit);
	printf("%s=%llu\n","dqb_isoftlimit", x->dqb_isoftlimit);
	printf("%s=%llu\n","dqb_curinodes",  x->dqb_curinodes);
	printf("%s=%llu\n","dqb_btime",      x->dqb_btime);
	printf("%s=%llu\n","dqb_itime",      x->dqb_itime);
	printf("%s=%u\n","dqb_valid",       x->dqb_valid);
	printf("%s=%u\n","dqb_padding",     x->dqb_padding);
}

void print_obd_uuid(struct obd_uuid *x)
{
	printf("uuid=%.*s\n", (int)sizeof(x->uuid), x->uuid);
}

void print_if_quotactl(struct if_quotactl *qctl)
{
	printf("%s=%u\n", "qc_cmd",   qctl->qc_cmd);
	printf("%s=%u\n", "qc_type",  qctl->qc_type);
	printf("%s=%u\n", "qc_id",    qctl->qc_id);
	printf("%s=%u\n", "qc_stat",  qctl->qc_stat);
	printf("%s=%u\n", "qc_valid", qctl->qc_valid);
	printf("%s=%u\n", "qc_idx",   qctl->qc_idx);
	printf("obd_dqinfo=\n");
	print_obd_dqinfo(&(qctl->qc_dqinfo));
	printf("obd_dqblk=\n");
	print_obd_dqblk(&(qctl->qc_dqblk));
	printf("obd_type=%.*s", (int)sizeof(qctl->obd_type), qctl->obd_type);
	printf("obd_uuid=\n");
	print_obd_uuid(&(qctl->obd_uuid));
}


/**
 * Check if a user is exceeding a quota on a lustre file system
 */
int exceeding_quota(char *mnt, char *username)
{
	int rc;
	struct if_quotactl *qctl;

	/* qctl should be zeros to start */
	qctl = calloc(1,sizeof(*qctl));    
	qctl->qc_cmd  = LUSTRE_Q_GETQUOTA;  // get info for specific user/groupt/etc.
	qctl->qc_cmd  = LUSTRE_Q_GETINFO;  // get info for specific user/groupt/etc.
	qctl->qc_type = USRQUOTA;           // get info for user

	/* use $UID not $LOGNAME */ 
	rc = name2uid(&(qctl->qc_id), username);
	if (rc != 0) {
		fprintf(stderr, "exceeding quota: user \"%s\" not found\n",
			username);
		return rc;
	}

	/* set dqb_valid so that only the mds is queried */
	qctl->qc_dqblk.dqb_valid = 1 << 10;

	printf("before sending:\n");
	print_if_quotactl(qctl);

	
	// goes to mdt across network
	rc = llapi_quotactl(mnt, qctl);
	if (rc == -1) {
		rc = -errno;
		return rc;
	}

	printf("\nafter sending:\n");	
	print_if_quotactl(qctl);

	return rc;
}



int main(int argc, char *argv[])
{
	char *mnt = argv[1];
	char *username = argv[2];
	exceeding_quota(mnt, username);
	return 0;
}
