/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
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
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * Implementation of cl_io for LOV layer.
 *
 *   Author: Nikita Danilov <nikita.danilov@sun.com>
 */

#define DEBUG_SUBSYSTEM S_LOV

#include "lov_cl_internal.h"

/** \addtogroup lov lov @{ */

static void lov_sub_enter(struct lov_io_sub *sub)
{
        ENTRY;
        if (sub->sub_reenter++ == 0) {
                sub->sub_cookie = cl_env_reenter();
                cl_env_implant(sub->sub_env, &sub->sub_refcheck2);
        }
        EXIT;
}

static void lov_sub_exit(struct lov_io_sub *sub)
{
        ENTRY;
        if (--sub->sub_reenter == 0) {
                cl_env_unplant(sub->sub_env, &sub->sub_refcheck2);
                cl_env_reexit(sub->sub_cookie);
        }
        EXIT;
}

static void lov_io_sub_fini(const struct lu_env *env, struct lov_io *lio,
                            struct lov_io_sub *sub)
{
        ENTRY;
        if (sub->sub_io != NULL) {
                if (sub->sub_io_initialized) {
                        lov_sub_enter(sub);
                        cl_io_fini(sub->sub_env, sub->sub_io);
                        lov_sub_exit(sub);
                        sub->sub_io_initialized = 0;
                        lio->lis_active_subios--;
                }
                if (sub->sub_stripe == lio->lis_single_subio_index)
                        lio->lis_single_subio_index = -1;
                else if (!sub->sub_borrowed)
                        OBD_FREE_PTR(sub->sub_io);
                sub->sub_io = NULL;
        }
        if (sub->sub_env != NULL && !IS_ERR(sub->sub_env)) {
                if (!sub->sub_borrowed)
                        cl_env_put(sub->sub_env, &sub->sub_refcheck);
                sub->sub_env = NULL;
        }
        EXIT;
}

static void lov_io_sub_inherit(struct cl_io *io, struct lov_io *lio,
                               int stripe, loff_t start, loff_t end)
{
        struct lov_stripe_md *lsm    = lov_r0(lio->lis_object)->lo_lsm;
        struct cl_io         *parent = lio->lis_cl.cis_io;

        switch(io->ci_type) {
        case CIT_TRUNC: {
                size_t new_size = parent->u.ci_truncate.tr_size;

                new_size = lov_size_to_stripe(lsm, new_size, stripe);
                io->u.ci_truncate.tr_capa = parent->u.ci_truncate.tr_capa;
                io->u.ci_truncate.tr_size = new_size;
                break;
        }
        case CIT_FAULT: {
                struct cl_object *obj = parent->ci_obj;
                loff_t off = cl_offset(obj, parent->u.ci_fault.ft_index);

                io->u.ci_fault = parent->u.ci_fault;
                off = lov_size_to_stripe(lsm, off, stripe);
                io->u.ci_fault.ft_index = cl_index(obj, off);
                break;
        }
        case CIT_READ:
        case CIT_WRITE: {
                io->u.ci_rw.crw_pos = start;
                io->u.ci_rw.crw_count = end - start;
                break;
        }
        default:
                break;
        }
}

static int lov_io_sub_init(const struct lu_env *env, struct lov_io *lio,
                           struct lov_io_sub *sub)
{
        struct lov_object *lov = lio->lis_object;
        struct lov_device *ld  = lu2lov_dev(lov2cl(lov)->co_lu.lo_dev);
        struct cl_io      *sub_io;
        struct cl_object  *sub_obj;
        struct cl_io      *io  = lio->lis_cl.cis_io;

        int stripe = sub->sub_stripe;
        int result;

        LASSERT(sub->sub_io == NULL);
        LASSERT(sub->sub_env == NULL);
        LASSERT(sub->sub_stripe < lio->lis_stripe_count);
        ENTRY;

        result = 0;
        sub->sub_io_initialized = 0;
        sub->sub_borrowed = 0;

        /*
         * First sub-io. Use ->lis_single_subio and current environment, to
         * avoid dynamic allocation.
         */
        if (lio->lis_active_subios == 0) {
                sub->sub_io = &lio->lis_single_subio;
                lio->lis_single_subio_index = stripe;
                sub->sub_env = cl_env_get(&sub->sub_refcheck);
                LASSERT(sub->sub_env == env);
        } else if (lio->lis_mem_frozen) {
                LASSERT(mutex_is_locked(&ld->ld_mutex));
                sub->sub_io  = &ld->ld_emrg[stripe]->emrg_subio;
                sub->sub_env = ld->ld_emrg[stripe]->emrg_env;
                sub->sub_borrowed = 1;
        } else {
                void *cookie;

                /* obtain new environment */
                cookie = cl_env_reenter();
                sub->sub_env = cl_env_get(&sub->sub_refcheck);
                cl_env_reexit(cookie);

                OBD_ALLOC_PTR(sub->sub_io);
                if (IS_ERR(sub->sub_env))
                        result = PTR_ERR(sub->sub_env);
                else if (sub->sub_io == NULL)
                        result = -ENOMEM;
        }

        if (result == 0) {
                sub_obj = lovsub2cl(lov_r0(lov)->lo_sub[stripe]);
                sub_io  = sub->sub_io;

                sub_io->ci_obj    = sub_obj;
                sub_io->ci_result = 0;

                sub_io->ci_parent  = io;
                sub_io->ci_lockreq = io->ci_lockreq;
                sub_io->ci_type    = io->ci_type;

                lov_sub_enter(sub);
                result = cl_io_sub_init(sub->sub_env, sub_io,
                                        io->ci_type, sub_obj);
                lov_sub_exit(sub);
                if (result >= 0) {
                        lio->lis_active_subios++;
                        sub->sub_io_initialized = 1;
                        result = 0;
                }
        }
        if (result != 0)
                lov_io_sub_fini(env, lio, sub);
        RETURN(result);
}

struct lov_io_sub *lov_sub_get(const struct lu_env *env,
                               struct lov_io *lio, int stripe)
{
        int rc;
        struct lov_io_sub *sub = &lio->lis_subs[stripe];

        LASSERT(stripe < lio->lis_stripe_count);
        ENTRY;

        if (!sub->sub_io_initialized) {
                sub->sub_stripe = stripe;
                rc = lov_io_sub_init(env, lio, sub);
        } else
                rc = 0;
        if (rc == 0)
                lov_sub_enter(sub);
        else
                sub = ERR_PTR(rc);
        RETURN(sub);
}

void lov_sub_put(struct lov_io_sub *sub)
{
        lov_sub_exit(sub);
}

/*****************************************************************************
 *
 * Lov io operations.
 *
 */

static int lov_page_stripe(const struct cl_page *page)
{
        struct lovsub_object *subobj;

        ENTRY;
        subobj = lu2lovsub(
                lu_object_locate(page->cp_child->cp_obj->co_lu.lo_header,
                                 &lovsub_device_type));
        LASSERT(subobj != NULL);
        RETURN(subobj->lso_index);
}

struct lov_io_sub *lov_page_subio(const struct lu_env *env, struct lov_io *lio,
                                  const struct cl_page_slice *slice)
{
        struct lov_stripe_md *lsm  = lov_r0(lio->lis_object)->lo_lsm;
        struct cl_page       *page = slice->cpl_page;
        int stripe;

        LASSERT(lio->lis_cl.cis_io != NULL);
        LASSERT(cl2lov(slice->cpl_obj) == lio->lis_object);
        LASSERT(lsm != NULL);
        LASSERT(lio->lis_nr_subios > 0);
        ENTRY;

        stripe = lov_page_stripe(page);
        RETURN(lov_sub_get(env, lio, stripe));
}


static int lov_io_subio_init(const struct lu_env *env, struct lov_io *lio,
                             struct cl_io *io)
{
        struct lov_object    *lov = lio->lis_object;
        struct lov_stripe_md *lsm = lov_r0(lov)->lo_lsm;
        int result;

        LASSERT(lio->lis_object != NULL);
        ENTRY;

        /*
         * Need to be optimized, we can't afford to allocate a piece of memory
         * when writing a page. -jay
         */
        OBD_ALLOC(lio->lis_subs,
                  lsm->lsm_stripe_count * sizeof lio->lis_subs[0]);
        if (lio->lis_subs != NULL) {
                lio->lis_nr_subios = lio->lis_stripe_count;
                lio->lis_single_subio_index = -1;
                lio->lis_active_subios = 0;
                result = 0;
        } else
                result = -ENOMEM;
        RETURN(result);
}

static void lov_io_slice_init(struct lov_io *lio,
                              struct lov_object *obj, struct cl_io *io)
{
        struct lov_stripe_md *lsm = lov_r0(obj)->lo_lsm;

        LASSERT(lsm != NULL);
        ENTRY;

        io->ci_result = 0;
        lio->lis_object = obj;
        lio->lis_stripe_count = lsm->lsm_stripe_count;

        switch (io->ci_type) {
        case CIT_READ:
        case CIT_WRITE:
                lio->lis_pos = io->u.ci_rw.crw_pos;
                lio->lis_endpos = io->u.ci_rw.crw_pos + io->u.ci_rw.crw_count;
                lio->lis_io_endpos = lio->lis_endpos;
                if (cl_io_is_append(io)) {
                        LASSERT(io->ci_type == CIT_WRITE);
                        lio->lis_pos = 0;
                        lio->lis_endpos = OBD_OBJECT_EOF;
                }
                break;

        case CIT_TRUNC:
                lio->lis_pos = io->u.ci_truncate.tr_size;
                lio->lis_endpos = OBD_OBJECT_EOF;
                break;

        case CIT_FAULT: {
                pgoff_t index = io->u.ci_fault.ft_index;
                lio->lis_pos = cl_offset(io->ci_obj, index);
                lio->lis_endpos = cl_offset(io->ci_obj, index + 1);
                break;
        }

        case CIT_MISC:
                lio->lis_pos = 0;
                lio->lis_endpos = OBD_OBJECT_EOF;
                break;

        default:
                LBUG();
        }

        EXIT;
}

static void lov_io_fini(const struct lu_env *env, const struct cl_io_slice *ios)
{
        struct lov_io *lio = cl2lov_io(env, ios);
        int i;

        ENTRY;
        if (lio->lis_subs != NULL) {
                for (i = 0; i < lio->lis_nr_subios; i++)
                        lov_io_sub_fini(env, lio, &lio->lis_subs[i]);
                OBD_FREE(lio->lis_subs,
                         lio->lis_nr_subios * sizeof lio->lis_subs[0]);
                lio->lis_nr_subios = 0;
        }
        EXIT;
}

static obd_off lov_offset_mod(obd_off val, int delta)
{
        if (val != OBD_OBJECT_EOF)
                val += delta;
        return val;
}

static int lov_io_iter_init(const struct lu_env *env,
                            const struct cl_io_slice *ios)
{
        struct lov_io        *lio = cl2lov_io(env, ios);
        struct lov_stripe_md *lsm = lov_r0(lio->lis_object)->lo_lsm;
        struct lov_io_sub    *sub;
        obd_off endpos;
        obd_off start;
        obd_off end;
        int stripe;
        int rc = 0;

        ENTRY;
        endpos = lov_offset_mod(lio->lis_endpos, -1);
        for (stripe = 0; stripe < lio->lis_stripe_count; stripe++) {
                if (!lov_stripe_intersects(lsm, stripe, lio->lis_pos,
                                           endpos, &start, &end))
                        continue;

                end = lov_offset_mod(end, +1);
                sub = lov_sub_get(env, lio, stripe);
                if (!IS_ERR(sub)) {
                        lov_io_sub_inherit(sub->sub_io, lio, stripe,
                                           start, end);
                        rc = cl_io_iter_init(sub->sub_env, sub->sub_io);
                        lov_sub_put(sub);
                        CDEBUG(D_VFSTRACE, "shrink: %i [%llu, %llu)\n",
                               stripe, start, end);
                } else
                        rc = PTR_ERR(sub);

                if (!rc)
                        list_add_tail(&sub->sub_linkage, &lio->lis_active);
                else
                        break;
        }
        RETURN(rc);
}

static int lov_io_rw_iter_init(const struct lu_env *env,
                               const struct cl_io_slice *ios)
{
        struct lov_io        *lio = cl2lov_io(env, ios);
        struct cl_io         *io  = ios->cis_io;
        struct lov_stripe_md *lsm = lov_r0(cl2lov(ios->cis_obj))->lo_lsm;
        loff_t start = io->u.ci_rw.crw_pos;
        loff_t next;
        int ssize = lsm->lsm_stripe_size;

        LASSERT(io->ci_type == CIT_READ || io->ci_type == CIT_WRITE);
        ENTRY;

        /* fast path for common case. */
        if (lio->lis_nr_subios != 1 && !cl_io_is_append(io)) {

                do_div(start, ssize);
                next = (start + 1) * ssize;
                if (next <= start * ssize)
                        next = ~0ull;

                io->ci_continue = next < lio->lis_io_endpos;
                io->u.ci_rw.crw_count = min_t(loff_t, lio->lis_io_endpos,
                                              next) - io->u.ci_rw.crw_pos;
                lio->lis_pos    = io->u.ci_rw.crw_pos;
                lio->lis_endpos = io->u.ci_rw.crw_pos + io->u.ci_rw.crw_count;
                CDEBUG(D_VFSTRACE, "stripe: %llu chunk: [%llu, %llu) %llu\n",
                       (__u64)start, lio->lis_pos, lio->lis_endpos,
                       (__u64)lio->lis_io_endpos);
        }
        /*
         * XXX The following call should be optimized: we know, that
         * [lio->lis_pos, lio->lis_endpos) intersects with exactly one stripe.
         */
        RETURN(lov_io_iter_init(env, ios));
}

static int lov_io_call(const struct lu_env *env, struct lov_io *lio,
                       int (*iofunc)(const struct lu_env *, struct cl_io *))
{
        struct lov_io_sub *sub;
        int rc = 0;

        ENTRY;
        list_for_each_entry(sub, &lio->lis_active, sub_linkage) {
                lov_sub_enter(sub);
                rc = iofunc(sub->sub_env, sub->sub_io);
                lov_sub_exit(sub);
                if (rc)
                        break;
        }
        RETURN(rc);
}

static int lov_io_lock(const struct lu_env *env, const struct cl_io_slice *ios)
{
        ENTRY;
        RETURN(lov_io_call(env, cl2lov_io(env, ios), cl_io_lock));
}

static int lov_io_start(const struct lu_env *env, const struct cl_io_slice *ios)
{
        ENTRY;
        RETURN(lov_io_call(env, cl2lov_io(env, ios), cl_io_start));
}

static int lov_io_end_wrapper(const struct lu_env *env, struct cl_io *io)
{
        ENTRY;
        /*
         * It's possible that lov_io_start() wasn't called against this
         * sub-io, either because previous sub-io failed, or upper layer
         * completed IO.
         */
        if (io->ci_state == CIS_IO_GOING)
                cl_io_end(env, io);
        else
                io->ci_state = CIS_IO_FINISHED;
        RETURN(0);
}

static int lov_io_iter_fini_wrapper(const struct lu_env *env, struct cl_io *io)
{
        cl_io_iter_fini(env, io);
        RETURN(0);
}

static int lov_io_unlock_wrapper(const struct lu_env *env, struct cl_io *io)
{
        cl_io_unlock(env, io);
        RETURN(0);
}

static void lov_io_end(const struct lu_env *env, const struct cl_io_slice *ios)
{
        int rc;

        rc = lov_io_call(env, cl2lov_io(env, ios), lov_io_end_wrapper);
        LASSERT(rc == 0);
}

static void lov_io_iter_fini(const struct lu_env *env,
                             const struct cl_io_slice *ios)
{
        struct lov_io *lio = cl2lov_io(env, ios);
        int rc;

        ENTRY;
        rc = lov_io_call(env, lio, lov_io_iter_fini_wrapper);
        LASSERT(rc == 0);
        while (!list_empty(&lio->lis_active))
                list_del_init(lio->lis_active.next);
        EXIT;
}

static void lov_io_unlock(const struct lu_env *env,
                          const struct cl_io_slice *ios)
{
        int rc;

        ENTRY;
        rc = lov_io_call(env, cl2lov_io(env, ios), lov_io_unlock_wrapper);
        LASSERT(rc == 0);
        EXIT;
}


static struct cl_page_list *lov_io_submit_qin(struct lov_device *ld,
                                              struct cl_page_list *qin,
                                              int idx, int alloc)
{
        return alloc ? &qin[idx] : &ld->ld_emrg[idx]->emrg_page_list;
}

/**
 * lov implementation of cl_operations::cio_submit() method. It takes a list
 * of pages in \a queue, splits it into per-stripe sub-lists, invokes
 * cl_io_submit() on underlying devices to submit sub-lists, and then splices
 * everything back.
 *
 * Major complication of this function is a need to handle memory cleansing:
 * cl_io_submit() is called to write out pages as a part of VM memory
 * reclamation, and hence it may not fail due to memory shortages (system
 * dead-locks otherwise). To deal with this, some resources (sub-lists,
 * sub-environment, etc.) are allocated per-device on "startup" (i.e., in a
 * not-memory cleansing context), and in case of memory shortage, these
 * pre-allocated resources are used by lov_io_submit() under
 * lov_device::ld_mutex mutex.
 */
static int lov_io_submit(const struct lu_env *env,
                         const struct cl_io_slice *ios,
                         enum cl_req_type crt, struct cl_2queue *queue)
{
        struct lov_io          *lio = cl2lov_io(env, ios);
        struct lov_object      *obj = lio->lis_object;
        struct lov_device       *ld = lu2lov_dev(lov2cl(obj)->co_lu.lo_dev);
        struct cl_page_list    *qin = &queue->c2_qin;
        struct cl_2queue      *cl2q = &lov_env_info(env)->lti_cl2q;
        struct cl_page_list *stripes_qin = NULL;
        struct cl_page *page;
        struct cl_page *tmp;
        int stripe;

#define QIN(stripe) lov_io_submit_qin(ld, stripes_qin, stripe, alloc)

        int rc = 0;
        int alloc =
#if defined(__KERNEL__) && defined(__linux__)
                !(current->flags & PF_MEMALLOC);
#else
                1;
#endif
        ENTRY;
        if (lio->lis_active_subios == 1) {
                int idx = lio->lis_single_subio_index;
                struct lov_io_sub *sub;

                LASSERT(idx < lio->lis_nr_subios);
                sub = lov_sub_get(env, lio, idx);
                LASSERT(!IS_ERR(sub));
                LASSERT(sub->sub_io == &lio->lis_single_subio);
                rc = cl_io_submit_rw(sub->sub_env, sub->sub_io, crt, queue);
                lov_sub_put(sub);
                RETURN(rc);
        }

        LASSERT(lio->lis_subs != NULL);
        if (alloc) {
                OBD_ALLOC(stripes_qin,
                          sizeof(*stripes_qin) * lio->lis_nr_subios);
                if (stripes_qin == NULL)
                        RETURN(-ENOMEM);

                for (stripe = 0; stripe < lio->lis_nr_subios; stripe++)
                        cl_page_list_init(&stripes_qin[stripe]);
        } else {
                /*
                 * If we get here, it means pageout & swap doesn't help.
                 * In order to not make things worse, even don't try to
                 * allocate the memory with __GFP_NOWARN. -jay
                 */
                mutex_lock(&ld->ld_mutex);
                lio->lis_mem_frozen = 1;
        }

        cl_2queue_init(cl2q);
        cl_page_list_for_each_safe(page, tmp, qin) {
                stripe = lov_page_stripe(page);
                cl_page_list_move(QIN(stripe), qin, page);
        }

        for (stripe = 0; stripe < lio->lis_nr_subios; stripe++) {
                struct lov_io_sub   *sub;
                struct cl_page_list *sub_qin = QIN(stripe);

                if (list_empty(&sub_qin->pl_pages))
                        continue;

                cl_page_list_splice(sub_qin, &cl2q->c2_qin);
                sub = lov_sub_get(env, lio, stripe);
                if (!IS_ERR(sub)) {
                        rc = cl_io_submit_rw(sub->sub_env, sub->sub_io,
                                             crt, cl2q);
                        lov_sub_put(sub);
                } else
                        rc = PTR_ERR(sub);
                cl_page_list_splice(&cl2q->c2_qin,  &queue->c2_qin);
                cl_page_list_splice(&cl2q->c2_qout, &queue->c2_qout);
                if (rc != 0)
                        break;
        }

        for (stripe = 0; stripe < lio->lis_nr_subios; stripe++) {
                struct cl_page_list *sub_qin = QIN(stripe);

                if (list_empty(&sub_qin->pl_pages))
                        continue;

                cl_page_list_splice(sub_qin, qin);
        }

        if (alloc) {
                OBD_FREE(stripes_qin,
                         sizeof(*stripes_qin) * lio->lis_nr_subios);
        } else {
                int i;

                for (i = 0; i < lio->lis_nr_subios; i++) {
                        struct cl_io *cio = lio->lis_subs[i].sub_io;

                        if (cio && cio == &ld->ld_emrg[i]->emrg_subio)
                                lov_io_sub_fini(env, lio, &lio->lis_subs[i]);
                }
                lio->lis_mem_frozen = 0;
                mutex_unlock(&ld->ld_mutex);
        }

        RETURN(rc);
#undef QIN
}

static int lov_io_prepare_write(const struct lu_env *env,
                                const struct cl_io_slice *ios,
                                const struct cl_page_slice *slice,
                                unsigned from, unsigned to)
{
        struct lov_io     *lio      = cl2lov_io(env, ios);
        struct cl_page    *sub_page = lov_sub_page(slice);
        struct lov_io_sub *sub;
        int result;

        ENTRY;
        sub = lov_page_subio(env, lio, slice);
        if (!IS_ERR(sub)) {
                result = cl_io_prepare_write(sub->sub_env, sub->sub_io,
                                             sub_page, from, to);
                lov_sub_put(sub);
        } else
                result = PTR_ERR(sub);
        RETURN(result);
}

static int lov_io_commit_write(const struct lu_env *env,
                               const struct cl_io_slice *ios,
                               const struct cl_page_slice *slice,
                               unsigned from, unsigned to)
{
        struct lov_io     *lio      = cl2lov_io(env, ios);
        struct cl_page    *sub_page = lov_sub_page(slice);
        struct lov_io_sub *sub;
        int result;

        ENTRY;
        sub = lov_page_subio(env, lio, slice);
        if (!IS_ERR(sub)) {
                result = cl_io_commit_write(sub->sub_env, sub->sub_io,
                                            sub_page, from, to);
                lov_sub_put(sub);
        } else
                result = PTR_ERR(sub);
        RETURN(result);
}

static int lov_io_fault_start(const struct lu_env *env,
                              const struct cl_io_slice *ios)
{
        struct cl_fault_io *fio;
        struct lov_io      *lio;
        struct lov_io_sub  *sub;

        ENTRY;
        fio = &ios->cis_io->u.ci_fault;
        lio = cl2lov_io(env, ios);
        sub = lov_sub_get(env, lio, lov_page_stripe(fio->ft_page));
        sub->sub_io->u.ci_fault.ft_nob = fio->ft_nob;
        lov_sub_put(sub);
        RETURN(lov_io_start(env, ios));
}

static const struct cl_io_operations lov_io_ops = {
        .op = {
                [CIT_READ] = {
                        .cio_fini      = lov_io_fini,
                        .cio_iter_init = lov_io_rw_iter_init,
                        .cio_iter_fini = lov_io_iter_fini,
                        .cio_lock      = lov_io_lock,
                        .cio_unlock    = lov_io_unlock,
                        .cio_start     = lov_io_start,
                        .cio_end       = lov_io_end
                },
                [CIT_WRITE] = {
                        .cio_fini      = lov_io_fini,
                        .cio_iter_init = lov_io_rw_iter_init,
                        .cio_iter_fini = lov_io_iter_fini,
                        .cio_lock      = lov_io_lock,
                        .cio_unlock    = lov_io_unlock,
                        .cio_start     = lov_io_start,
                        .cio_end       = lov_io_end
                },
                [CIT_TRUNC] = {
                        .cio_fini      = lov_io_fini,
                        .cio_iter_init = lov_io_iter_init,
                        .cio_iter_fini = lov_io_iter_fini,
                        .cio_lock      = lov_io_lock,
                        .cio_unlock    = lov_io_unlock,
                        .cio_start     = lov_io_start,
                        .cio_end       = lov_io_end
                },
                [CIT_FAULT] = {
                        .cio_fini      = lov_io_fini,
                        .cio_iter_init = lov_io_iter_init,
                        .cio_iter_fini = lov_io_iter_fini,
                        .cio_lock      = lov_io_lock,
                        .cio_unlock    = lov_io_unlock,
                        .cio_start     = lov_io_fault_start,
                        .cio_end       = lov_io_end
                },
                [CIT_MISC] = {
                        .cio_fini   = lov_io_fini
                }
        },
        .req_op = {
                 [CRT_READ] = {
                         .cio_submit    = lov_io_submit
                 },
                 [CRT_WRITE] = {
                         .cio_submit    = lov_io_submit
                 }
         },
        .cio_prepare_write = lov_io_prepare_write,
        .cio_commit_write  = lov_io_commit_write
};

/*****************************************************************************
 *
 * Empty lov io operations.
 *
 */

static void lov_empty_io_fini(const struct lu_env *env,
                              const struct cl_io_slice *ios)
{
        ENTRY;
        EXIT;
}

static void lov_empty_impossible(const struct lu_env *env,
                                 struct cl_io_slice *ios)
{
        LBUG();
}

#define LOV_EMPTY_IMPOSSIBLE ((void *)lov_empty_impossible)

/**
 * An io operation vector for files without stripes.
 */
static const struct cl_io_operations lov_empty_io_ops = {
        .op = {
                [CIT_READ] = {
#if 0
                        .cio_fini       = lov_empty_io_fini,
                        .cio_iter_init  = LOV_EMPTY_IMPOSSIBLE,
                        .cio_lock       = LOV_EMPTY_IMPOSSIBLE,
                        .cio_start      = LOV_EMPTY_IMPOSSIBLE,
                        .cio_end        = LOV_EMPTY_IMPOSSIBLE
#endif
                },
                [CIT_WRITE] = {
                        .cio_fini      = lov_empty_io_fini,
                        .cio_iter_init = LOV_EMPTY_IMPOSSIBLE,
                        .cio_lock      = LOV_EMPTY_IMPOSSIBLE,
                        .cio_start     = LOV_EMPTY_IMPOSSIBLE,
                        .cio_end       = LOV_EMPTY_IMPOSSIBLE
                },
                [CIT_TRUNC] = {
                        .cio_fini      = lov_empty_io_fini,
                        .cio_iter_init = LOV_EMPTY_IMPOSSIBLE,
                        .cio_lock      = LOV_EMPTY_IMPOSSIBLE,
                        .cio_start     = LOV_EMPTY_IMPOSSIBLE,
                        .cio_end       = LOV_EMPTY_IMPOSSIBLE
                },
                [CIT_FAULT] = {
                        .cio_fini      = lov_empty_io_fini,
                        .cio_iter_init = LOV_EMPTY_IMPOSSIBLE,
                        .cio_lock      = LOV_EMPTY_IMPOSSIBLE,
                        .cio_start     = LOV_EMPTY_IMPOSSIBLE,
                        .cio_end       = LOV_EMPTY_IMPOSSIBLE
                },
                [CIT_MISC] = {
                        .cio_fini   = lov_empty_io_fini
                }
        },
        .req_op = {
                 [CRT_READ] = {
                         .cio_submit    = LOV_EMPTY_IMPOSSIBLE
                 },
                 [CRT_WRITE] = {
                         .cio_submit    = LOV_EMPTY_IMPOSSIBLE
                 }
         },
        .cio_commit_write = LOV_EMPTY_IMPOSSIBLE
};

int lov_io_init_raid0(const struct lu_env *env, struct cl_object *obj,
                      struct cl_io *io)
{
        struct lov_io       *lio = lov_env_io(env);
        struct lov_object   *lov = cl2lov(obj);

        ENTRY;
        CFS_INIT_LIST_HEAD(&lio->lis_active);
        lov_io_slice_init(lio, lov, io);
        if (io->ci_result == 0) {
                LASSERT(lov_r0(lov)->lo_lsm != NULL);
                io->ci_result = lov_io_subio_init(env, lio, io);
                if (io->ci_result == 0)
                        cl_io_slice_add(io, &lio->lis_cl, obj, &lov_io_ops);
        }
        RETURN(io->ci_result);
}

int lov_io_init_empty(const struct lu_env *env, struct cl_object *obj,
                      struct cl_io *io)
{
        struct lov_io *lio = lov_env_io(env);
        int result;

        ENTRY;
        switch (io->ci_type) {
        default:
                LBUG();
        case CIT_MISC:
        case CIT_READ:
                result = 0;
                break;
        case CIT_WRITE:
        case CIT_TRUNC:
                result = -EBADF;
                break;
        case CIT_FAULT:
                result = -EFAULT;
                CERROR("Page fault on a file without stripes: "DFID"\n",
                       PFID(lu_object_fid(&obj->co_lu)));
                break;
        }
        if (result == 0)
                cl_io_slice_add(io, &lio->lis_cl, obj, &lov_empty_io_ops);
        io->ci_result = result;
        RETURN(result != 0);
}

/** @} lov */
