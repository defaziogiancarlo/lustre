/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <grp.h>
#include <sys/wait.h>

#define DEBUG 0

#ifndef NGROUPS_MAX
#define NGROUPS_MAX 32
#endif

static const char usage[] =
"Usage: %s -u user_id [-g grp_id ] [ -G[gid0,gid1,...] ] command\n"
"  -u user_id           switch to UID user_id\n"
"  -g grp_id            switch to GID grp_id\n"
"  -G[gid0,gid1,...]    set supplementary groups\n";

void Usage_and_abort(const char *name)
{
        fprintf(stderr, usage, name);
        exit(-1);
}

int main(int argc, char **argv)
{
        char **my_argv, *name = argv[0], *grp;
        int status, c, i;
        int gid_is_set = 0, uid_is_set = 0, num_supp = -1;
        uid_t user_id = 0;
        gid_t grp_id = 0, supp_groups[NGROUPS_MAX] = { 0 };

        if (argc == 1)
                Usage_and_abort(name);

        // get UID and GID
        while ((c = getopt(argc, argv, "+u:g:hG::")) != -1) {
                switch (c) {
                case 'u':
                        user_id = (uid_t)atoi(optarg);
                        uid_is_set = 1;
                        if (!gid_is_set)
                                grp_id = user_id;
                        break;

                case 'g':
                        grp_id = (gid_t)atoi(optarg);
                        gid_is_set = 1;
                        break;

                case 'G':
                        num_supp = 0;
                        if (optarg == NULL || !isdigit(optarg[0]))
                                break;
                        while ((grp = strsep(&optarg, ",")) != NULL) {
                                printf("adding supp group %d\n", atoi(grp));
                                supp_groups[num_supp++] = atoi(grp);
                                if (num_supp >= NGROUPS_MAX)
                                        break;
                        }
                        break;

                default:
                case 'h':
                        Usage_and_abort(name);
                        break;
                }
        }

        if (!uid_is_set)
                Usage_and_abort(name);

        if (optind == argc) {
                fputs("Must specify command to run.\n", stderr);
                Usage_and_abort(name);
        }

        // assemble the command
        my_argv = (char**)malloc(sizeof(char*)*(argc+1-optind));
        if (my_argv == NULL) {
                fprintf(stderr, "Error in allocating memory. (%s)\n",
                        strerror(errno));
                exit(-1);
        }

        for (i = optind; i < argc; i++) {
                my_argv[i-optind] = argv[i];
                //printf("%s\n",my_argv[i-optind]);
        }
        my_argv[i-optind] = NULL;

#if DEBUG
        system("whoami");
#endif

        // set GID
        status = setregid(grp_id, grp_id);
        if (status == -1) {
                 fprintf(stderr, "Cannot change grp_ID to %d, errno=%d (%s)\n",
                         grp_id, errno, strerror(errno) );
                 exit(-1);
        }

        if (num_supp >= 0) {
                status = setgroups(num_supp, supp_groups);
                if (status == -1) {
                        perror("setting supplementary groups");
                        exit(-1);
                }
        }

        // set UID
        status = setreuid(user_id, user_id );
        if(status == -1) {
                  fprintf(stderr,"Cannot change user_ID to %d, errno=%d (%s)\n",
                           user_id, errno, strerror(errno) );
                  exit(-1);
        }

        fprintf(stderr, "running as UID %d, GID %d", user_id, grp_id);
        for (i = 0; i < num_supp; i++)
                fprintf(stderr, ":%d", supp_groups[i]);
        fprintf(stderr, "\n");

        for (i = 0; i < argc - optind; i++)
                 fprintf(stderr, " [%s]", my_argv[i]);

        fprintf(stderr, "\n");
        fflush(stderr);

        // The command to be run
        execvp(my_argv[0], my_argv);
        fprintf(stderr, "execvp fails running %s\n", my_argv[0]);
        exit(-1);
}

