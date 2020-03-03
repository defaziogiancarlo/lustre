/* get the quota data and look at it
 *
 * also do some performance testing
 */

#include <lustre/lustreapi.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <stdlib.h>

#define GET_BIT(x,n) ((int)(((x) >> (n)) & 1))

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

void print_flags(__u32 flags)
{
	int i;
	char *flag_names[] = {"lqe_enforced", "lqe_uptodate", "lqe_edquot",
			      "lqe_gl", "lqe_nopreacq", "lqe_is_default"};

	for (i = 0; i < 6; ++i) {
		printf("%s=%d\n", flag_names[i], GET_BIT(flags, i));
	}
}

void print_obd_dqinfo(struct obd_dqinfo *x)
{
	printf("%s=%llu\n","dqi_bgrace", x->dqi_bgrace);
	printf("%s=%llu\n","dqi_igrace", x->dqi_igrace);
	printf("%s=0x%.6x\n","dqi_flags",   x->dqi_flags);
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

void set_if_quotactl(struct if_quotactl *qctl, char *username, int valid)
{
	memset(qctl, 0, sizeof(*qctl));
	qctl->qc_cmd  = LUSTRE_Q_GETQUOTA;  // get info for specific user/groupt/etc.
//	qctl->qc_cmd  = LUSTRE_Q_GETINFO;  // get info for specific user/groupt/etc.
	qctl->qc_type = USRQUOTA;           // get info for user

	/* use $UID not $LOGNAME */ 
	if (name2uid(&(qctl->qc_id), username) != 0) {
		fprintf(stderr, "exceeding quota: user \"%s\" not found\n",
			username);
	}

	/* set dqb_valid so that only the mds is queried */
	if (valid)
		qctl->qc_dqblk.dqb_valid = 1;
}


struct if_quotactl *init_if_quotactl(char *username, int valid)
{
 	struct if_quotactl *qctl = malloc(sizeof(*qctl));    
	set_if_quotactl(qctl, username, valid);
	return qctl;
}




/* check if the dqb_valid flags actually changes lookup time */
int perf_test(char *mnt, char *username, int iters, int valid)
{
	struct timespec tsi;
	struct timespec tsf;

	int i = 0;
	int rc = 0;
	struct if_quotactl *qctl;
	qctl = init_if_quotactl(username, valid);

	for (i = 0; i < iters; ++i) {
		clock_gettime(CLOCK_MONOTONIC, &tsi);
		rc = llapi_quotactl(mnt, qctl);
		clock_gettime(CLOCK_MONOTONIC, &tsf);
		if (rc == -1) {
			rc = -errno;
			return rc;
		}
		double elaps_s = difftime(tsf.tv_sec, tsi.tv_sec);
		long elaps_ns = tsf.tv_nsec - tsi.tv_nsec;
		printf("%.13f\n",  elaps_s + ((double)elaps_ns) / 1.0e9);
	}

	return rc;
}
	








/**
 * Check if a user is exceeding a quota on a lustre file system
 */
int exceeding_quota(char *mnt, char *username)
{
	int rc;
	struct if_quotactl *qctl;

	qctl = init_if_quotactl(username, 1);
	

	printf("before sending:\n");
	print_if_quotactl(qctl);
	//print_flags(qctl->qc_dqinfo.dqi_flags);
	
	// goes to mdt across network
	rc = llapi_quotactl(mnt, qctl);
	if (rc == -1) {
		rc = -errno;
		return rc;
	}

	printf("\nafter sending:\n");	
	print_if_quotactl(qctl);
	//print_flags(qctl->qc_dqinfo.dqi_flags);
	return rc;
}



int main(int argc, char *argv[])
{
	char *mnt = argv[1];
	char *username = argv[2];
	int iters = atoi(argv[3]);
	int valid = atoi(argv[4]);
//	exceeding_quota(mnt, username);
	perf_test(mnt, username, iters, valid);
	return 0;
}
