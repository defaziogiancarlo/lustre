/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *   This file is part of Lustre, http://www.lustre.org
 */
#ifndef _OBDCTL_H_
#define _OBDCTL_H_

#ifndef __KERNEL__
#include <liblustre.h>
#endif

#include <linux/lustre_lib.h>
#include <linux/lustre_idl.h>
#include <linux/lustre_dlm.h>
#include <linux/lustre_cfg.h>

/* obd.c */
int do_disconnect(char *func, int verbose);
int obd_initialize(int argc, char **argv);
void obd_finalize(int argc, char **argv);


int jt_opt_device(int argc, char **argv);
int jt_opt_threads(int argc, char **argv);
int jt_opt_net(int argc, char **argv);

int jt_obd_device(int argc, char **argv);
int jt_obd_connect(int argc, char **argv);
int jt_obd_disconnect(int argc, char **argv);
int jt_obd_detach(int argc, char **argv);
int jt_obd_cleanup(int argc, char **argv);
int jt_obd_no_transno(int argc, char **argv);
int jt_obd_set_readonly(int argc, char **argv);
int jt_obd_abort_recovery(int argc, char **argv);
int jt_obd_list(int argc, char **argv);
int jt_obd_create(int argc, char **argv);
int jt_obd_setattr(int argc, char **argv);
int jt_obd_test_setattr(int argc, char **argv);
int jt_obd_destroy(int argc, char **argv);
int jt_obd_getattr(int argc, char **argv);
int jt_obd_test_getattr(int argc, char **argv);
int jt_obd_test_brw(int argc, char **argv);
int jt_obd_get_stripe(int argc, char **argv);
int jt_obd_set_stripe(int argc, char **argv);
int jt_obd_unset_stripe(int argc, char **argv);
int jt_obd_lov_getconfig(int argc, char **argv);
int jt_obd_test_ldlm(int argc, char **argv);
int jt_obd_ldlm_regress_start(int argc, char **argv);
int jt_obd_ldlm_regress_stop(int argc, char **argv);
int jt_obd_dump_ldlm(int argc, char **argv);
int jt_obd_activate(int argc, char **argv);
int jt_obd_deactivate(int argc, char **argv);
int jt_obd_recover(int argc, char **argv);
int jt_obd_mdc_lookup(int argc, char **argv);
int jt_get_version(int argc, char **argv);
int jt_obd_close_uuid(int argc, char **argv);
int jt_cfg_record(int argc, char **argv);
int jt_cfg_endrecord(int argc, char **argv);
int jt_cfg_parse(int argc, char **argv);
int jt_cfg_dump_log(int argc, char **argv);
int jt_cfg_clear_log(int argc, char **argv);

int jt_llog_catlist(int argc, char **argv);
int jt_llog_info(int argc, char **argv);
int jt_llog_print(int argc, char **argv);
int jt_llog_cancel(int argc, char **argv);
int jt_llog_remove(int argc, char **argv);
int jt_llog_check(int argc, char **argv);

int lcfg_ioctl(char * func, int dev_id, struct lustre_cfg *lcfg);
int parse_devname(char *func, char *name);
char *jt_cmdname(char *func);


/* lustre_cfg.h */
int jt_lcfg_device(int argc, char **argv);
int jt_lcfg_newdev(int argc, char **argv);
int jt_lcfg_attach(int argc, char **argv);
int jt_lcfg_setup(int argc, char **argv);
int jt_lcfg_add_uuid(int argc, char **argv);
int jt_lcfg_del_uuid(int argc, char **argv);
int jt_lcfg_lov_setup(int argc, char **argv);
int jt_lcfg_lov_modify_tgts(int argc, char **argv);
int jt_lcfg_mount_option(int argc, char **argv);
int jt_lcfg_del_mount_option(int argc, char **argv);
int jt_lcfg_set_timeout(int argc, char **argv);
int jt_lcfg_set_lustre_upcall(int argc, char **argv);
int jt_lcfg_add_conn(int argc, char **argv);
int jt_lcfg_del_conn(int argc, char **argv);

int obd_add_uuid(char *uuid, ptl_nid_t nid, int nal);

#endif
