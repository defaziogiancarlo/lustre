/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2002 Cluster File Systems, Inc.
 *
 * This code is issued under the GNU General Public License.
 * See the file COPYING in this distribution
 *
 * by Cluster File Systems, Inc.
 * authors, Peter Braam <braam@clusterfs.com> & 
 * Phil Schwan <phil@clusterfs.com>
 */

#define EXPORT_SYMTAB

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/unistd.h>

#define DEBUG_SUBSYSTEM S_LDLM

#include <linux/obd_support.h>
#include <linux/obd_class.h>

#include <linux/lustre_dlm.h>

extern kmem_cache_t *ldlm_lock_slab;

static int ldlm_plain_compat(struct ldlm_lock *a, struct ldlm_lock *b);
static int ldlm_intent_compat(struct ldlm_lock *a, struct ldlm_lock *b);

ldlm_res_compat ldlm_res_compat_table [] = {
        [LDLM_PLAIN] ldlm_plain_compat,
        [LDLM_EXTENT] ldlm_extent_compat,
        [LDLM_MDSINTENT] ldlm_intent_compat
};

ldlm_res_policy ldlm_res_policy_table [] = {
        [LDLM_PLAIN] NULL,
        [LDLM_EXTENT] ldlm_extent_policy,
        [LDLM_MDSINTENT] NULL
};

static int ldlm_plain_compat(struct ldlm_lock *a, struct ldlm_lock *b)
{
        return lockmode_compat(a->l_req_mode, b->l_req_mode);
}

static int ldlm_intent_compat(struct ldlm_lock *a, struct ldlm_lock *b)
{
        LBUG();
        return 0;
}

static struct ldlm_lock *ldlm_lock_new(struct ldlm_lock *parent,
                                       struct ldlm_resource *resource,
                                       ldlm_mode_t mode)
{
        struct ldlm_lock *lock;

        if (resource == NULL)
                LBUG();

        lock = kmem_cache_alloc(ldlm_lock_slab, SLAB_KERNEL);
        if (lock == NULL)
                return NULL;

        memset(lock, 0, sizeof(*lock));
        lock->l_resource = resource;
        lock->l_req_mode = mode;
        INIT_LIST_HEAD(&lock->l_children);

        if (parent != NULL) {
                lock->l_parent = parent;
                list_add(&lock->l_childof, &parent->l_children);
        }

        return lock;
}

void ldlm_lock_free(struct ldlm_lock *lock)
{
        kmem_cache_free(ldlm_lock_slab, lock);
}

static int ldlm_lock_compat(struct ldlm_lock *lock)
{
        struct list_head *tmp;
        int rc = 0;

        list_for_each(tmp, &lock->l_resource->lr_granted) {
                struct ldlm_lock *child;
                ldlm_res_compat compat;

                child = list_entry(tmp, struct ldlm_lock, l_res_link);

                compat = ldlm_res_compat_table[child->l_resource->lr_type];
                if (compat(child, lock) ||
                    lockmode_compat(child->l_req_mode, lock->l_req_mode))
                        continue;

                rc = 1;

                if (child->l_blocking_ast != NULL)
                        child->l_blocking_ast(child, lock, child->l_data,
                                              child->l_data_len);
        }

        return rc;
}

static void ldlm_grant_lock(struct ldlm_resource *res, struct ldlm_lock *lock)
{
        ldlm_resource_add_lock(res, &res->lr_granted, lock);
        lock->l_granted_mode = lock->l_req_mode;

        if (lock->l_granted_mode < res->lr_most_restr)
                res->lr_most_restr = lock->l_granted_mode;

        if (lock->l_completion_ast)
                lock->l_completion_ast(lock, NULL, NULL, 0);
}

/* XXX: Revisit the error handling; we do not, for example, do
 * ldlm_resource_put()s in our error cases, and we probably leak any allocated
 * memory. */
ldlm_error_t ldlm_local_lock_enqueue(struct obd_device *obddev,
                                     __u32 ns_id,
                                     struct ldlm_handle *parent_lock_handle,
                                     __u64 *res_id,
                                     __u32 type,
                                     struct ldlm_extent *req_ex,
                                     ldlm_mode_t mode,
                                     int *flags,
                                     ldlm_lock_callback completion,
                                     ldlm_lock_callback blocking,
                                     void *data,
                                     __u32 data_len,
                                     struct ldlm_handle *lockh)
{
        struct ldlm_namespace *ns;
        struct ldlm_resource *res, *parent_res;
        struct ldlm_lock *lock, *parent_lock;
        struct ldlm_extent new_ex;
        int incompat = 0, rc;
        ldlm_res_policy policy;

        ENTRY;

        parent_lock = ldlm_handle2object(parent_lock_handle);
        if (parent_lock)
                parent_res = parent_lock->l_resource;
        else 
                parent_res = NULL;

        ns = ldlm_namespace_find(obddev, ns_id);
        if (ns == NULL || ns->ns_hash == NULL) 
                RETURN(-ELDLM_BAD_NAMESPACE);

        res = ldlm_resource_get(ns, parent_res, res_id, type, 1);
        if (res == NULL)
                RETURN(-ENOMEM);

        lock = ldlm_lock_new(parent_lock, res, mode);
        if (lock == NULL)
                RETURN(-ENOMEM);

        if ((policy = ldlm_res_policy_table[type])) {
                rc = policy(res, req_ex, &new_ex, mode, NULL);
                if (rc == ELDLM_LOCK_CHANGED) {
                        *flags |= LDLM_FL_LOCK_CHANGED;
                        memcpy(req_ex, &new_ex, sizeof(new_ex));
                }
        }

        if ((type == LDLM_EXTENT && !req_ex) ||
            (type != LDLM_EXTENT && req_ex))
                LBUG();
        if (req_ex)
                memcpy(&lock->l_extent, req_ex, sizeof(*req_ex));
        lock->l_data = data;
        lock->l_data_len = data_len;
        if ((*flags) & LDLM_FL_COMPLETION_AST)
                lock->l_completion_ast = completion;
        if ((*flags) & LDLM_FL_BLOCKING_AST)
                lock->l_blocking_ast = blocking;
        ldlm_object2handle(lock, lockh);
        spin_lock(&res->lr_lock);

        /* FIXME: We may want to optimize by checking lr_most_restr */

        if (!list_empty(&res->lr_converting)) {
                ldlm_resource_add_lock(res, res->lr_waiting.prev, lock);
                GOTO(out, rc = -ELDLM_BLOCK_CONV);
        }
        if (!list_empty(&res->lr_waiting)) {
                ldlm_resource_add_lock(res, res->lr_waiting.prev, lock);
                GOTO(out, rc = -ELDLM_BLOCK_WAIT);
        }

        incompat = ldlm_lock_compat(lock);
        if (incompat) {
                ldlm_resource_add_lock(res, res->lr_waiting.prev, lock);
                GOTO(out, rc = -ELDLM_BLOCK_GRANTED);
        }

        ldlm_grant_lock(res, lock);
        GOTO(out, rc = ELDLM_OK);

 out:
        spin_unlock(&res->lr_lock);
        return rc;
}

static int ldlm_reprocess_queue(struct ldlm_resource *res,
                                struct list_head *converting)
{
        struct list_head *tmp, *pos;
        int incompat = 0;

        list_for_each_safe(tmp, pos, converting) { 
                struct ldlm_lock *pending;
                pending = list_entry(tmp, struct ldlm_lock, l_res_link);

                incompat = ldlm_lock_compat(pending);
                if (incompat)
                        break;

                list_del(&pending->l_res_link); 
                ldlm_grant_lock(res, pending);
        }

        return incompat;
}

static void ldlm_reprocess_all(struct ldlm_resource *res)
{
        ldlm_reprocess_queue(res, &res->lr_converting);
        if (list_empty(&res->lr_converting))
                ldlm_reprocess_queue(res, &res->lr_waiting);
}

ldlm_error_t ldlm_local_lock_cancel(struct obd_device *obddev,
                                    struct ldlm_handle *lockh)
{
        struct ldlm_lock *lock;
        struct ldlm_resource *res;
        ENTRY;

        lock = ldlm_handle2object(lockh);
        res = lock->l_resource;

        ldlm_resource_del_lock(lock);

        ldlm_lock_free(lock);
        if (ldlm_resource_put(res))
                RETURN(ELDLM_OK);
        ldlm_reprocess_all(res);

        RETURN(ELDLM_OK);
}

ldlm_error_t ldlm_local_lock_convert(struct obd_device *obddev,
                                     struct ldlm_handle *lockh,
                                     int new_mode, int *flags)
{
        struct ldlm_lock *lock;
        struct ldlm_resource *res;
        ENTRY;

        lock = ldlm_handle2object(lockh);
        res = lock->l_resource;
        list_del(&lock->l_res_link);
        lock->l_req_mode = new_mode;

        list_add(&lock->l_res_link, res->lr_converting.prev);

        ldlm_reprocess_all(res);

        RETURN(ELDLM_OK);
}

void ldlm_lock_dump(struct ldlm_lock *lock)
{
        char ver[128];

        if (RES_VERSION_SIZE != 4)
                LBUG();

        snprintf(ver, sizeof(ver), "%x %x %x %x",
                 lock->l_version[0], lock->l_version[1],
                 lock->l_version[2], lock->l_version[3]);

        CDEBUG(D_OTHER, "  -- Lock dump: %p (%s)\n", lock, ver);
        CDEBUG(D_OTHER, "  Parent: %p\n", lock->l_parent);
        CDEBUG(D_OTHER, "  Resource: %p\n", lock->l_resource);
        CDEBUG(D_OTHER, "  Requested mode: %d, granted mode: %d\n",
               (int)lock->l_req_mode, (int)lock->l_granted_mode);
        if (lock->l_resource->lr_type == LDLM_EXTENT)
                CDEBUG(D_OTHER, "  Extent: %Lu -> %Lu\n",
                       lock->l_extent.start, lock->l_extent.end);
}
