/**
 * Enhanced Seccomp Filter DB
 *
 * Copyright (c) 2012,2016 Red Hat <pmoore@redhat.com>
 * Author: Paul Moore <paul@paul-moore.com>
 */

/*
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses>.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <seccomp.h>

#include "arch.h"
#include "db.h"
#include "system.h"

/* state values */
#define _DB_STA_VALID			0xA1B2C3D4
#define _DB_STA_FREED			0x1A2B3C4D

/* the priority field is fairly simple - without any user hints, or in the case
 * of a hint "tie", we give higher priority to syscalls with less chain nodes
 * (filter is easier to evaluate) */
#define _DB_PRI_MASK_CHAIN		0x0000FFFF
#define _DB_PRI_MASK_USER		0x00FF0000
#define _DB_PRI_USER(x)			(((x) << 16) & _DB_PRI_MASK_USER)

/* private structure for tracking the state of the sub-tree "pruning" */
struct db_prune_state {
	bool prefix_exist;
	bool prefix_new;
	bool matched;
};

static unsigned int _db_tree_free(struct db_arg_chain_tree *tree);

/**
 * Do not call this function directly, use _db_tree_free() instead
 */
static unsigned int __db_tree_free(struct db_arg_chain_tree *tree)
{
	int cnt;

	if (tree == NULL || --(tree->refcnt) > 0)
		return 0;

	/* we assume the caller has ensured that 'tree->lvl_prv == NULL' */
	cnt = __db_tree_free(tree->lvl_nxt);
	cnt += _db_tree_free(tree->nxt_t);
	cnt += _db_tree_free(tree->nxt_f);

	free(tree);
	return cnt + 1;
}

/**
 * Free a syscall filter argument chain tree
 * @param tree the argument chain list
 *
 * This function frees a tree and returns the number of nodes freed.
 *
 */
static unsigned int _db_tree_free(struct db_arg_chain_tree *tree)
{
	struct db_arg_chain_tree *iter;

	if (tree == NULL)
		return 0;

	iter = tree;
	while (iter->lvl_prv != NULL)
		iter = iter->lvl_prv;

	return __db_tree_free(iter);
}

/**
 * Remove a node from an argument chain tree
 * @param tree the pointer to the tree
 * @param node the node to remove
 *
 * This function searches the tree looking for the node and removes it once
 * found.  Returns the number of nodes freed.
 *
 */
static unsigned int _db_tree_remove(struct db_arg_chain_tree **tree,
				    struct db_arg_chain_tree *node)
{
	int cnt = 0;
	struct db_arg_chain_tree *c_iter;

	if (tree == NULL || *tree == NULL || node == NULL)
		return 0;

	c_iter = *tree;
	while (c_iter->lvl_prv != NULL)
		c_iter = c_iter->lvl_prv;

	do {
		if (c_iter == node || db_chain_zombie(c_iter)) {
			/* remove from the tree */
			if (c_iter == *tree) {
				if (c_iter->lvl_prv != NULL)
					*tree = c_iter->lvl_prv;
				else
					*tree = c_iter->lvl_nxt;
			}
			if (c_iter->lvl_prv != NULL)
				c_iter->lvl_prv->lvl_nxt = c_iter->lvl_nxt;
			if (c_iter->lvl_nxt != NULL)
				c_iter->lvl_nxt->lvl_prv = c_iter->lvl_prv;

			/* free and return */
			c_iter->lvl_prv = NULL;
			c_iter->lvl_nxt = NULL;
			cnt += _db_tree_free(c_iter);
			return cnt;
		}

		/* check the true/false sub-trees */
		cnt += _db_tree_remove(&(c_iter->nxt_t), node);
		cnt += _db_tree_remove(&(c_iter->nxt_f), node);

		c_iter = c_iter->lvl_nxt;
	} while (c_iter != NULL);

	return cnt;
}

/**
 * Traverse a tree checking the action values
 * @param tree the pointer to the tree
 * @param action the action
 *
 * Traverse the tree inspecting each action to see if it matches the given
 * action.  Returns zero if all actions match the given action, negative values
 * on failure.
 *
 */
static int _db_tree_act_check(struct db_arg_chain_tree *tree, uint32_t action)
{
	int rc;
	struct db_arg_chain_tree *c_iter;

	if (tree == NULL)
		return 0;

	c_iter = tree;
	while (c_iter->lvl_prv != NULL)
		c_iter = c_iter->lvl_prv;

	do {
		if (c_iter->act_t_flg && c_iter->act_t != action)
			return -EEXIST;
		if (c_iter->act_f_flg && c_iter->act_f != action)
			return -EEXIST;

		rc = _db_tree_act_check(c_iter->nxt_t, action);
		if (rc < 0)
			return rc;
		rc = _db_tree_act_check(c_iter->nxt_f, action);
		if (rc < 0)
			return rc;

		c_iter = c_iter->lvl_nxt;
	} while (c_iter != NULL);

	return 0;
}

/**
 * Checks for a sub-tree match in an existing tree and prunes the tree
 * @param prev the head of the existing tree or sub-tree
 * @param existing the starting point into the existing tree
 * @param new pointer to the new tree
 * @param state pointer to the pruning state
 *
 * This function searches the existing and new trees trying to prune each to
 * eliminate redundancy.  Returns the number of nodes removed from the tree on
 * success, zero if no changes were made, and negative values if the new tree
 * should be discarded.
 *
 */
static int _db_tree_sub_prune(struct db_arg_chain_tree **prev,
			      struct db_arg_chain_tree *existing,
			      struct db_arg_chain_tree *new,
			      struct db_prune_state *state)
{
	int rc = 0;
	int rc_tmp;
	struct db_arg_chain_tree *ec_iter;
	struct db_arg_chain_tree *ec_iter_tmp;
	struct db_arg_chain_tree *c_iter;
	struct db_prune_state state_new;

	if (!state || !existing || !new)
		return 0;

	ec_iter = existing;
	c_iter = new;
	do {
		if (db_chain_eq(ec_iter, c_iter)) {
			/* equal */

			if (db_chain_leaf(c_iter)) {
				/* leaf */
				if (db_chain_eq_result(ec_iter, c_iter)) {
					/* identical results */
					if (prev != NULL)
						return _db_tree_remove(prev,
								       ec_iter);
					else
						return -1;
				}
				if (c_iter->act_t_flg && ec_iter->nxt_t) {
					/* new is shorter (true) */
					if (prev == NULL)
						return -1;
					rc += _db_tree_remove(&(ec_iter->nxt_t),
							      ec_iter->nxt_t);
					ec_iter->act_t = c_iter->act_t;
					ec_iter->act_t_flg = true;
				}
				if (c_iter->act_f_flg && ec_iter->nxt_f) {
					/* new is shorter (false) */
					if (prev == NULL)
						return -1;
					rc += _db_tree_remove(&(ec_iter->nxt_f),
							      ec_iter->nxt_f);
					ec_iter->act_f = c_iter->act_f;
					ec_iter->act_f_flg = true;
				}

				return rc;
			}

			if (c_iter->nxt_t && ec_iter->act_t_flg)
				/* existing is shorter (true) */
				return -1;
			if (c_iter->nxt_f && ec_iter->act_f_flg)
				/* existing is shorter (false) */
				return -1;

			if (c_iter->nxt_t) {
				state_new = *state;
				state_new.matched = true;
				rc_tmp = _db_tree_sub_prune((prev ?
							     &ec_iter : NULL),
							    ec_iter->nxt_t,
							    c_iter->nxt_t,
							    &state_new);
				rc += (rc_tmp > 0 ? rc_tmp : 0);
				if (state->prefix_new && rc_tmp < 0)
					return (rc > 0 ? rc : rc_tmp);
			}
			if (c_iter->nxt_f) {
				state_new = *state;
				state_new.matched = true;
				rc_tmp = _db_tree_sub_prune((prev ?
							     &ec_iter : NULL),
							    ec_iter->nxt_f,
							    c_iter->nxt_f,
							    &state_new);
				rc += (rc_tmp > 0 ? rc_tmp : 0);
				if (state->prefix_new && rc_tmp < 0)
					return (rc > 0 ? rc : rc_tmp);
			}
		} else if (db_chain_lt(ec_iter, c_iter)) {
			/* less than */
			if (state->matched || state->prefix_new)
				goto next;
			state_new = *state;
			state_new.prefix_exist = true;

			if (ec_iter->nxt_t) {
				rc_tmp = _db_tree_sub_prune((prev ?
							     &ec_iter : NULL),
							    ec_iter->nxt_t,
							    c_iter,
							    &state_new);
				rc += (rc_tmp > 0 ? rc_tmp : 0);
			}
			if (ec_iter->nxt_f) {
				rc_tmp = _db_tree_sub_prune((prev ?
							     &ec_iter : NULL),
							    ec_iter->nxt_f,
							    c_iter,
							    &state_new);
				rc += (rc_tmp > 0 ? rc_tmp : 0);
			}
		} else if (db_chain_gt(ec_iter, c_iter)) {
			/* greater than */
			if (state->matched || state->prefix_exist)
				goto next;
			state_new = *state;
			state_new.prefix_new = true;

			if (c_iter->nxt_t) {
				rc_tmp = _db_tree_sub_prune(NULL,
							    ec_iter,
							    c_iter->nxt_t,
							    &state_new);
				rc += (rc_tmp > 0 ? rc_tmp : 0);
				if (rc_tmp < 0)
					return (rc > 0 ? rc : rc_tmp);
			}
			if (c_iter->nxt_f) {
				rc_tmp = _db_tree_sub_prune(NULL,
							    ec_iter,
							    c_iter->nxt_f,
							    &state_new);
				rc += (rc_tmp > 0 ? rc_tmp : 0);
				if (rc_tmp < 0)
					return (rc > 0 ? rc : rc_tmp);
			}
		}

next:
		/* re-check current node and advance to the next node */
		if (db_chain_zombie(ec_iter)) {
			ec_iter_tmp = ec_iter->lvl_nxt;
			rc += _db_tree_remove(prev, ec_iter);
			ec_iter = ec_iter_tmp;
		} else
			ec_iter = ec_iter->lvl_nxt;
	} while (ec_iter);

	return rc;
}

/**
 * Free and reset the seccomp filter DB
 * @param db the seccomp filter DB
 *
 * This function frees any existing filters and resets the filter DB to a
 * default state; only the DB architecture is preserved.
 *
 */
static void _db_reset(struct db_filter *db)
{
	struct db_sys_list *s_iter;
	struct db_api_rule_list *r_iter;

	if (db == NULL)
		return;

	/* free any filters */
	if (db->syscalls != NULL) {
		s_iter = db->syscalls;
		while (s_iter != NULL) {
			db->syscalls = s_iter->next;
			_db_tree_free(s_iter->chains);
			free(s_iter);
			s_iter = db->syscalls;
		}
		db->syscalls = NULL;
	}

	/* free any rules */
	if (db->rules != NULL) {
		/* split the loop first then loop and free */
		db->rules->prev->next = NULL;
		r_iter = db->rules;
		while (r_iter != NULL) {
			db->rules = r_iter->next;
			free(r_iter->args);
			free(r_iter);
			r_iter = db->rules;
		}
		db->rules = NULL;
	}
}

/**
 * Intitalize a seccomp filter DB
 * @param arch the architecture definition
 *
 * This function initializes a seccomp filter DB and readies it for use.
 * Returns a pointer to the DB on success, NULL on failure.
 *
 */
static struct db_filter *_db_init(const struct arch_def *arch)
{
	struct db_filter *db;

	db = malloc(sizeof(*db));
	if (db == NULL)
		return NULL;

	/* clear the buffer for the first time and set the arch */
	memset(db, 0, sizeof(*db));
	db->arch = arch;

	/* reset the DB to a known state */
	_db_reset(db);

	return db;
}

/**
 * Destroy a seccomp filter DB
 * @param db the seccomp filter DB
 *
 * This function destroys a seccomp filter DB.  After calling this function,
 * the filter should no longer be referenced.
 *
 */
static void _db_release(struct db_filter *db)
{
	if (db == NULL)
		return;

	/* free and reset the DB */
	_db_reset(db);
	free(db);
}

/**
 * Destroy a seccomp filter snapshot
 * @param snap the seccomp filter snapshot
 *
 * This function destroys a seccomp filter snapshot.  After calling this
 * function, the snapshot should no longer be referenced.
 *
 */
static void _db_snap_release(struct db_filter_snap *snap)
{
	unsigned int iter;

	if (snap->filter_cnt > 0) {
		for (iter = 0; iter < snap->filter_cnt; iter++) {
			if (snap->filters[iter])
				_db_release(snap->filters[iter]);
		}
		free(snap->filters);
	}
	free(snap);
}

/**
 * Update the user specified portion of the syscall priority
 * @param db the seccomp filter db
 * @param syscall the syscall number
 * @param priority the syscall priority
 *
 * This function sets, or updates, the syscall priority; the highest priority
 * value between the existing and specified value becomes the new syscall
 * priority.  If the syscall entry does not already exist, a new phantom
 * syscall entry is created as a placeholder.  Returns zero on success,
 * negative values on failure.
 *
 */
static int _db_syscall_priority(struct db_filter *db,
				int syscall, uint8_t priority)
{
	unsigned int sys_pri = _DB_PRI_USER(priority);
	struct db_sys_list *s_new, *s_iter, *s_prev = NULL;

	assert(db != NULL);

	s_iter = db->syscalls;
	while (s_iter != NULL && s_iter->num < syscall) {
		s_prev = s_iter;
		s_iter = s_iter->next;
	}

	/* matched an existing syscall entry */
	if (s_iter != NULL && s_iter->num == syscall) {
		if (sys_pri > (s_iter->priority & _DB_PRI_MASK_USER)) {
			s_iter->priority &= (~_DB_PRI_MASK_USER);
			s_iter->priority |= sys_pri;
		}
		return 0;
	}

	/* no existing syscall entry - create a phantom entry */
	s_new = malloc(sizeof(*s_new));
	if (s_new == NULL)
		return -ENOMEM;
	memset(s_new, 0, sizeof(*s_new));
	s_new->num = syscall;
	s_new->priority = sys_pri;
	s_new->valid = false;

	/* add it before s_iter */
	if (s_prev != NULL) {
		s_new->next = s_prev->next;
		s_prev->next = s_new;
	} else {
		s_new->next = db->syscalls;
		db->syscalls = s_new;
	}

	return 0;
}

/**
 * Free and reset the seccomp filter collection
 * @param col the seccomp filter collection
 * @param def_action the default filter action
 *
 * This function frees any existing filter DBs and resets the collection to a
 * default state.  In the case of failure the filter collection may be in an
 * unknown state and should be released.  Returns zero on success, negative
 * values on failure.
 *
 */
int db_col_reset(struct db_filter_col *col, uint32_t def_action)
{
	unsigned int iter;
	struct db_filter *db;
	struct db_filter_snap *snap;

	if (col == NULL)
		return -EINVAL;

	/* free any filters */
	for (iter = 0; iter < col->filter_cnt; iter++)
		_db_release(col->filters[iter]);
	col->filter_cnt = 0;
	if (col->filters)
		free(col->filters);
	col->filters = NULL;

	/* set the endianess to undefined */
	col->endian = 0;

	/* set the default attribute values */
	col->attr.act_default = def_action;
	col->attr.act_badarch = SCMP_ACT_KILL;
	col->attr.nnp_enable = 1;
	col->attr.tsync_enable = 0;

	/* set the state */
	col->state = _DB_STA_VALID;

	/* reset the initial db */
	db = _db_init(arch_def_native);
	if (db == NULL)
		return -ENOMEM;
	if (db_col_db_add(col, db) < 0) {
		_db_release(db);
		return -ENOMEM;
	}

	/* reset the transactions */
	while (col->snapshots) {
		snap = col->snapshots;
		col->snapshots = snap->next;
		for (iter = 0; iter < snap->filter_cnt; iter++)
			_db_release(snap->filters[iter]);
		free(snap->filters);
		free(snap);
	}

	return 0;
}

/**
 * Intitalize a seccomp filter collection
 * @param def_action the default filter action
 *
 * This function initializes a seccomp filter collection and readies it for
 * use.  Returns a pointer to the collection on success, NULL on failure.
 *
 */
struct db_filter_col *db_col_init(uint32_t def_action)
{
	struct db_filter_col *col;

	col = malloc(sizeof(*col));
	if (col == NULL)
		return NULL;

	/* clear the buffer for the first time */
	memset(col, 0, sizeof(*col));

	/* reset the DB to a known state */
	if (db_col_reset(col, def_action) < 0)
		goto init_failure;

	return col;

init_failure:
	db_col_release(col);
	return NULL;
}

/**
 * Destroy a seccomp filter collection
 * @param col the seccomp filter collection
 *
 * This function destroys a seccomp filter collection.  After calling this
 * function, the filter should no longer be referenced.
 *
 */
void db_col_release(struct db_filter_col *col)
{
	unsigned int iter;

	if (col == NULL)
		return;

	/* set the state, just in case */
	col->state = _DB_STA_FREED;

	/* free any filters */
	for (iter = 0; iter < col->filter_cnt; iter++)
		_db_release(col->filters[iter]);
	col->filter_cnt = 0;
	if (col->filters)
		free(col->filters);
	col->filters = NULL;

	/* free the collection */
	free(col);
}

/**
 * Validate the seccomp action
 * @param action the seccomp action
 *
 * Verify that the given action is a valid seccomp action; return zero if
 * valid, -EINVAL if invalid.
 */
int db_action_valid(uint32_t action)
{
	if (action == SCMP_ACT_KILL)
		return 0;
	else if (action == SCMP_ACT_TRAP)
		return 0;
	else if ((action == SCMP_ACT_ERRNO(action & 0x0000ffff)) &&
		 ((action & 0x0000ffff) < MAX_ERRNO))
		return 0;
	else if (action == SCMP_ACT_TRACE(action & 0x0000ffff))
		return 0;
	else if (action == SCMP_ACT_ALLOW)
		return 0;

	return -EINVAL;
}

/**
 * Validate a filter collection
 * @param col the seccomp filter collection
 *
 * This function validates a seccomp filter collection.  Returns zero if the
 * collection is valid, negative values on failure.
 *
 */
int db_col_valid(struct db_filter_col *col)
{
	if (col != NULL && col->state == _DB_STA_VALID && col->filter_cnt > 0)
		return 0;
	return -EINVAL;
}

/**
 * Merge two filter collections
 * @param col_dst the destination filter collection
 * @param col_src the source filter collection
 *
 * This function merges two filter collections into the given destination
 * collection.  The source filter collection is no longer valid if the function
 * returns successfully.  Returns zero on success, negative values on failure.
 *
 */
int db_col_merge(struct db_filter_col *col_dst, struct db_filter_col *col_src)
{
	unsigned int iter_a, iter_b;
	struct db_filter **dbs;

	/* verify that the endianess is a match */
	if (col_dst->endian != col_src->endian)
		return -EEXIST;

	/* make sure we don't have any arch/filter collisions */
	for (iter_a = 0; iter_a < col_dst->filter_cnt; iter_a++) {
		for (iter_b = 0; iter_b < col_src->filter_cnt; iter_b++) {
			if (col_dst->filters[iter_a]->arch->token ==
			    col_src->filters[iter_b]->arch->token)
				return -EEXIST;
		}
	}

	/* expand the destination */
	dbs = realloc(col_dst->filters,
		      sizeof(struct db_filter *) *
		      (col_dst->filter_cnt + col_src->filter_cnt));
	if (dbs == NULL)
		return -ENOMEM;
	col_dst->filters = dbs;

	/* transfer the architecture filters */
	for (iter_a = col_dst->filter_cnt, iter_b = 0;
	     iter_b < col_src->filter_cnt; iter_a++, iter_b++) {
		col_dst->filters[iter_a] = col_src->filters[iter_b];
		col_dst->filter_cnt++;
	}

	/* free the source */
	col_src->filter_cnt = 0;
	db_col_release(col_src);

	return 0;
}

/**
 * Check to see if an architecture filter exists in the filter collection
 * @param col the seccomp filter collection
 * @param arch_token the architecture token
 *
 * Iterate through the given filter collection checking to see if a filter
 * exists for the specified architecture.  Returns -EEXIST if a filter is found,
 * zero if a matching filter does not exist.
 *
 */
int db_col_arch_exist(struct db_filter_col *col, uint32_t arch_token)
{
	unsigned int iter;

	for (iter = 0; iter < col->filter_cnt; iter++)
		if (col->filters[iter]->arch->token == arch_token)
			return -EEXIST;

	return 0;
}

/**
 * Get a filter attribute
 * @param col the seccomp filter collection
 * @param attr the filter attribute
 * @param value the filter attribute value
 *
 * Get the requested filter attribute and provide it via @value.  Returns zero
 * on success, negative values on failure.
 *
 */
int db_col_attr_get(const struct db_filter_col *col,
		    enum scmp_filter_attr attr, uint32_t *value)
{
	int rc = 0;

	switch (attr) {
	case SCMP_FLTATR_ACT_DEFAULT:
		*value = col->attr.act_default;
		break;
	case SCMP_FLTATR_ACT_BADARCH:
		*value = col->attr.act_badarch;
		break;
	case SCMP_FLTATR_CTL_NNP:
		*value = col->attr.nnp_enable;
		break;
	case SCMP_FLTATR_CTL_TSYNC:
		*value = col->attr.tsync_enable;
		break;
	default:
		rc = -EEXIST;
		break;
	}

	return rc;
}

/**
 * Set a filter attribute
 * @param col the seccomp filter collection
 * @param attr the filter attribute
 * @param value the filter attribute value
 *
 * Set the requested filter attribute with the given value.  Returns zero on
 * success, negative values on failure.
 *
 */
int db_col_attr_set(struct db_filter_col *col,
		    enum scmp_filter_attr attr, uint32_t value)
{
	int rc = 0;

	switch (attr) {
	case SCMP_FLTATR_ACT_DEFAULT:
		/* read only */
		return -EACCES;
		break;
	case SCMP_FLTATR_ACT_BADARCH:
		if (db_action_valid(value) == 0)
			col->attr.act_badarch = value;
		else
			return -EINVAL;
		break;
	case SCMP_FLTATR_CTL_NNP:
		col->attr.nnp_enable = (value ? 1 : 0);
		break;
	case SCMP_FLTATR_CTL_TSYNC:
		rc = sys_chk_seccomp_flag(SECCOMP_FILTER_FLAG_TSYNC);
		if (rc == 1) {
			/* supported */
			rc = 0;
			col->attr.tsync_enable = (value ? 1 : 0);
		} else if (rc == 0)
			/* unsupported */
			rc = -EOPNOTSUPP;
		break;
	default:
		rc = -EEXIST;
		break;
	}

	return rc;
}

/**
 * Add a new architecture filter to a filter collection
 * @param col the seccomp filter collection
 * @param arch the architecture
 *
 * This function adds a new architecture filter DB to an existing seccomp
 * filter collection assuming there isn't a filter DB already present with the
 * same architecture.  Returns zero on success, negative values on failure.
 *
 */
int db_col_db_new(struct db_filter_col *col, const struct arch_def *arch)
{
	int rc;
	struct db_filter *db;

	db = _db_init(arch);
	if (db == NULL)
		return -ENOMEM;
	rc = db_col_db_add(col, db);
	if (rc < 0)
		_db_release(db);

	return rc;
}

/**
 * Add a new filter DB to a filter collection
 * @param col the seccomp filter collection
 * @param db the seccomp filter DB
 *
 * This function adds an existing seccomp filter DB to an existing seccomp
 * filter collection assuming there isn't a filter DB already present with the
 * same architecture.  Returns zero on success, negative values on failure.
 *
 */
int db_col_db_add(struct db_filter_col *col, struct db_filter *db)
{
	struct db_filter **dbs;

	if (col->endian != 0 && col->endian != db->arch->endian)
		return -EEXIST;

	if (db_col_arch_exist(col, db->arch->token))
		return -EEXIST;

	dbs = realloc(col->filters,
		      sizeof(struct db_filter *) * (col->filter_cnt + 1));
	if (dbs == NULL)
		return -ENOMEM;
	col->filters = dbs;
	col->filter_cnt++;
	col->filters[col->filter_cnt - 1] = db;
	if (col->endian == 0)
		col->endian = db->arch->endian;

	return 0;
}

/**
 * Remove a filter DB from a filter collection
 * @param col the seccomp filter collection
 * @param arch_token the architecture token
 *
 * This function removes an existing seccomp filter DB from an existing seccomp
 * filter collection.  Returns zero on success, negative values on failure.
 *
 */
int db_col_db_remove(struct db_filter_col *col, uint32_t arch_token)
{
	unsigned int iter;
	unsigned int found;
	struct db_filter **dbs;

	if ((col->filter_cnt <= 0) || (db_col_arch_exist(col, arch_token) == 0))
		return -EINVAL;

	for (found = 0, iter = 0; iter < col->filter_cnt; iter++) {
		if (found)
			col->filters[iter - 1] = col->filters[iter];
		else if (col->filters[iter]->arch->token == arch_token) {
			_db_release(col->filters[iter]);
			found = 1;
		}
	}
	col->filters[--col->filter_cnt] = NULL;

	if (col->filter_cnt > 0) {
		/* NOTE: if we can't do the realloc it isn't fatal, we just
		 *       have some extra space allocated */
		dbs = realloc(col->filters,
			      sizeof(struct db_filter *) * col->filter_cnt);
		if (dbs != NULL)
			col->filters = dbs;
	} else {
		/* this was the last filter so free all the associated memory
		 * and reset the endian token */
		free(col->filters);
		col->filters = NULL;
		col->endian = 0;
	}

	return 0;
}

/**
 * Test if the argument filter can be skipped because it's a tautology
 * @param arg argument filter
 *
 * If this argument filter applied to the lower 32 bit can be skipped this
 * function returns false.
 *
 */
static bool _db_arg_cmp_need_lo(const struct db_api_arg *arg)
{
	if (arg->op == SCMP_CMP_MASKED_EQ && D64_LO(arg->mask) == 0)
		return false;

	return true;
}

/**
 * Test if the argument filter can be skipped because it's a tautology
 * @param arg argument filter
 *
 * If this argument filter applied to the upper 32 bit can be skipped this
 * function returns false.
 *
 */
static bool _db_arg_cmp_need_hi(const struct db_api_arg *arg)
{
	if (arg->op == SCMP_CMP_MASKED_EQ && D64_HI(arg->mask) == 0)
		return false;

	return true;
}

/**
 * Fixup the node based on the op/mask
 * @param node the chain node
 *
 * Ensure the datum is masked as well.
 *
 */
static void _db_node_mask_fixup(struct db_arg_chain_tree *node)
{
	node->datum &= node->mask;
}

/**
 * Generate a new filter rule for a 64 bit system
 * @param arch the architecture definition
 * @param action the filter action
 * @param syscall the syscall number
 * @param chain argument filter chain
 *
 * This function generates a new syscall filter for a 64 bit system. Returns
 * zero on success, negative values on failure.
 *
 */
static struct db_sys_list *_db_rule_gen_64(const struct arch_def *arch,
					   uint32_t action,
					   unsigned int syscall,
					   struct db_api_arg *chain)
{
	unsigned int iter;
	int chain_len_max;
	struct db_sys_list *s_new;
	struct db_arg_chain_tree *c_iter_hi = NULL, *c_iter_lo = NULL;
	struct db_arg_chain_tree *c_prev_hi = NULL, *c_prev_lo = NULL;
	bool tf_flag;

	s_new = malloc(sizeof(*s_new));
	if (s_new == NULL)
		return NULL;
	memset(s_new, 0, sizeof(*s_new));
	s_new->num = syscall;
	s_new->valid = true;
	/* run through the argument chain */
	chain_len_max = arch_arg_count_max(arch);
	if (chain_len_max < 0)
		goto gen_64_failure;
	for (iter = 0; iter < chain_len_max; iter++) {
		if (chain[iter].valid == 0)
			continue;

		/* TODO: handle the case were either hi or lo isn't needed */

		/* skip generating instruction which are no-ops */
		if (!_db_arg_cmp_need_hi(&chain[iter]) &&
		    !_db_arg_cmp_need_lo(&chain[iter]))
			continue;

		c_iter_hi = malloc(sizeof(*c_iter_hi));
		if (c_iter_hi == NULL)
			goto gen_64_failure;
		memset(c_iter_hi, 0, sizeof(*c_iter_hi));
		c_iter_hi->refcnt = 1;
		c_iter_lo = malloc(sizeof(*c_iter_lo));
		if (c_iter_lo == NULL) {
			free(c_iter_hi);
			goto gen_64_failure;
		}
		memset(c_iter_lo, 0, sizeof(*c_iter_lo));
		c_iter_lo->refcnt = 1;

		/* link this level to the previous level */
		if (c_prev_lo != NULL) {
			if (!tf_flag) {
				c_prev_lo->nxt_f = c_iter_hi;
				c_prev_hi->nxt_f = c_iter_hi;
				c_iter_hi->refcnt++;
			} else
				c_prev_lo->nxt_t = c_iter_hi;
		} else
			s_new->chains = c_iter_hi;
		s_new->node_cnt += 2;

		/* set the arg, op, and datum fields */
		c_iter_hi->arg = chain[iter].arg;
		c_iter_lo->arg = chain[iter].arg;
		c_iter_hi->arg_offset = arch_arg_offset_hi(arch,
							   c_iter_hi->arg);
		c_iter_lo->arg_offset = arch_arg_offset_lo(arch,
							   c_iter_lo->arg);
		switch (chain[iter].op) {
		case SCMP_CMP_GT:
			c_iter_hi->op = SCMP_CMP_GE;
			c_iter_lo->op = SCMP_CMP_GT;
			tf_flag = true;
			break;
		case SCMP_CMP_NE:
			c_iter_hi->op = SCMP_CMP_EQ;
			c_iter_lo->op = SCMP_CMP_EQ;
			tf_flag = false;
			break;
		case SCMP_CMP_LT:
			c_iter_hi->op = SCMP_CMP_GE;
			c_iter_lo->op = SCMP_CMP_GE;
			tf_flag = false;
			break;
		case SCMP_CMP_LE:
			c_iter_hi->op = SCMP_CMP_GE;
			c_iter_lo->op = SCMP_CMP_GT;
			tf_flag = false;
			break;
		default:
			c_iter_hi->op = chain[iter].op;
			c_iter_lo->op = chain[iter].op;
			tf_flag = true;
		}
		c_iter_hi->mask = D64_HI(chain[iter].mask);
		c_iter_lo->mask = D64_LO(chain[iter].mask);
		c_iter_hi->datum = D64_HI(chain[iter].datum);
		c_iter_lo->datum = D64_LO(chain[iter].datum);

		/* fixup the mask/datum */
		_db_node_mask_fixup(c_iter_hi);
		_db_node_mask_fixup(c_iter_lo);

		/* link the hi and lo chain nodes */
		c_iter_hi->nxt_t = c_iter_lo;

		c_prev_hi = c_iter_hi;
		c_prev_lo = c_iter_lo;
	}
	if (c_iter_lo != NULL) {
		/* set the leaf node */
		if (!tf_flag) {
			c_iter_lo->act_f_flg = true;
			c_iter_lo->act_f = action;
			c_iter_hi->act_f_flg = true;
			c_iter_hi->act_f = action;
		} else {
			c_iter_lo->act_t_flg = true;
			c_iter_lo->act_t = action;
		}
	} else
		s_new->action = action;

	return s_new;

gen_64_failure:
	/* free the new chain and its syscall struct */
	_db_tree_free(s_new->chains);
	free(s_new);
	return NULL;
}

/**
 * Generate a new filter rule for a 32 bit system
 * @param arch the architecture definition
 * @param action the filter action
 * @param syscall the syscall number
 * @param chain argument filter chain
 *
 * This function generates a new syscall filter for a 32 bit system. Returns
 * zero on success, negative values on failure.
 *
 */
static struct db_sys_list *_db_rule_gen_32(const struct arch_def *arch,
					   uint32_t action,
					   unsigned int syscall,
					   struct db_api_arg *chain)
{
	unsigned int iter;
	int chain_len_max;
	struct db_sys_list *s_new;
	struct db_arg_chain_tree *c_iter = NULL, *c_prev = NULL;
	bool tf_flag;

	s_new = malloc(sizeof(*s_new));
	if (s_new == NULL)
		return NULL;
	memset(s_new, 0, sizeof(*s_new));
	s_new->num = syscall;
	s_new->valid = true;
	/* run through the argument chain */
	chain_len_max = arch_arg_count_max(arch);
	if (chain_len_max < 0)
		goto gen_32_failure;
	for (iter = 0; iter < chain_len_max; iter++) {
		if (chain[iter].valid == 0)
			continue;

		/* skip generating instructions which are no-ops */
		if (!_db_arg_cmp_need_lo(&chain[iter]))
			continue;

		c_iter = malloc(sizeof(*c_iter));
		if (c_iter == NULL)
			goto gen_32_failure;
		memset(c_iter, 0, sizeof(*c_iter));
		c_iter->refcnt = 1;
		c_iter->arg = chain[iter].arg;
		c_iter->arg_offset = arch_arg_offset(arch, c_iter->arg);
		c_iter->op = chain[iter].op;
		/* implicitly strips off the upper 32 bit */
		c_iter->mask = chain[iter].mask;
		c_iter->datum = chain[iter].datum;

		/* link in the new node and update the chain */
		if (c_prev != NULL) {
			if (tf_flag)
				c_prev->nxt_t = c_iter;
			else
				c_prev->nxt_f = c_iter;
		} else
			s_new->chains = c_iter;
		s_new->node_cnt++;

		/* rewrite the op to reduce the op/datum combos */
		switch (c_iter->op) {
		case SCMP_CMP_NE:
			c_iter->op = SCMP_CMP_EQ;
			tf_flag = false;
			break;
		case SCMP_CMP_LT:
			c_iter->op = SCMP_CMP_GE;
			tf_flag = false;
			break;
		case SCMP_CMP_LE:
			c_iter->op = SCMP_CMP_GT;
			tf_flag = false;
			break;
		default:
			tf_flag = true;
		}

		/* fixup the mask/datum */
		_db_node_mask_fixup(c_iter);

		c_prev = c_iter;
	}
	if (c_iter != NULL) {
		/* set the leaf node */
		if (tf_flag) {
			c_iter->act_t_flg = true;
			c_iter->act_t = action;
		} else {
			c_iter->act_f_flg = true;
			c_iter->act_f = action;
		}
	} else
		s_new->action = action;

	return s_new;

gen_32_failure:
	/* free the new chain and its syscall struct */
	_db_tree_free(s_new->chains);
	free(s_new);
	return NULL;
}

/**
 * Add a new rule to the seccomp filter DB
 * @param db the seccomp filter db
 * @param rule the filter rule
 *
 * This function adds a new syscall filter to the seccomp filter DB, adding to
 * the existing filters for the syscall, unless no argument specific filters
 * are present (filtering only on the syscall).  When adding new chains, the
 * shortest chain, or most inclusive filter match, will be entered into the
 * filter DB. Returns zero on success, negative values on failure.
 *
 */
int db_rule_add(struct db_filter *db, const struct db_api_rule_list *rule)
{
	int rc = -ENOMEM;
	int syscall = rule->syscall;
	uint32_t action = rule->action;
	struct db_api_arg *chain = rule->args;
	struct db_sys_list *s_new, *s_iter, *s_prev = NULL;
	struct db_arg_chain_tree *c_iter = NULL, *c_prev = NULL;
	struct db_arg_chain_tree *ec_iter;
	struct db_prune_state state;
	bool rm_flag = false;
	unsigned int new_chain_cnt = 0;
	unsigned int n_cnt;

	assert(db != NULL);

	/* do all our possible memory allocation up front so we don't have to
	 * worry about failure once we get to the point where we start updating
	 * the filter db */
	if (db->arch->size == ARCH_SIZE_64)
		s_new = _db_rule_gen_64(db->arch, action, syscall, chain);
	else if (db->arch->size == ARCH_SIZE_32)
		s_new = _db_rule_gen_32(db->arch, action, syscall, chain);
	else
		return -EFAULT;
	if (s_new == NULL)
		return -ENOMEM;
	new_chain_cnt = s_new->node_cnt;

	/* no more failures allowed after this point that would result in the
	 * stored filter being in an inconsistent state */

	/* find a matching syscall/chain or insert a new one */
	s_iter = db->syscalls;
	while (s_iter != NULL && s_iter->num < syscall) {
		s_prev = s_iter;
		s_iter = s_iter->next;
	}
add_reset:
	s_new->node_cnt = new_chain_cnt;
	s_new->priority = _DB_PRI_MASK_CHAIN - s_new->node_cnt;
	c_prev = NULL;
	c_iter = s_new->chains;
	if (s_iter != NULL)
		ec_iter = s_iter->chains;
	else
		ec_iter = NULL;
	if (s_iter == NULL || s_iter->num != syscall) {
		/* new syscall, add before s_iter */
		if (s_prev != NULL) {
			s_new->next = s_prev->next;
			s_prev->next = s_new;
		} else {
			s_new->next = db->syscalls;
			db->syscalls = s_new;
		}
		return 0;
	} else if (s_iter->chains == NULL) {
		if (rm_flag || !s_iter->valid) {
			/* we are here because our previous pass cleared the
			 * entire syscall chain when searching for a subtree
			 * match or the existing syscall entry is a phantom,
			 * so either way add the new chain */
			s_iter->chains = s_new->chains;
			s_iter->action = s_new->action;
			s_iter->node_cnt = s_new->node_cnt;
			if (s_iter->valid)
				s_iter->priority = s_new->priority;
			s_iter->valid = true;
			free(s_new);
			rc = 0;
			goto add_priority_update;
		} else
			/* syscall exists without any chains - existing filter
			 * is at least as large as the new entry so cleanup and
			 * exit */
			goto add_free_ok;
	} else if (s_iter->chains != NULL && s_new->chains == NULL) {
		/* syscall exists with chains but the new filter has no chains
		 * so we need to clear the existing chains and exit */
		_db_tree_free(s_iter->chains);
		s_iter->chains = NULL;
		s_iter->node_cnt = 0;
		s_iter->action = action;
		goto add_free_ok;
	}

	/* check for sub-tree matches */
	memset(&state, 0, sizeof(state));
	rc = _db_tree_sub_prune(&(s_iter->chains), ec_iter, c_iter, &state);
	if (rc > 0) {
		rm_flag = true;
		s_iter->node_cnt -= rc;
		goto add_reset;
	} else if (rc < 0)
		goto add_free_ok;

	/* syscall exists and has at least one existing chain - start at the
	 * top and walk the two chains */
	do {
		/* insert the new rule into the existing tree */
		if (db_chain_eq(c_iter, ec_iter)) {
			/* found a matching node on this chain level */
			if (db_chain_action(c_iter) &&
			    db_chain_action(ec_iter)) {
				/* both are "action" nodes */
				if (c_iter->act_t_flg && ec_iter->act_t_flg) {
					if (ec_iter->act_t != action)
						goto add_free_exist;
				} else if (c_iter->act_t_flg) {
					ec_iter->act_t_flg = true;
					ec_iter->act_t = action;
				}
				if (c_iter->act_f_flg && ec_iter->act_f_flg) {
					if (ec_iter->act_f != action)
						goto add_free_exist;
				} else if (c_iter->act_f_flg) {
					ec_iter->act_f_flg = true;
					ec_iter->act_f = action;
				}
				if (ec_iter->act_t_flg == ec_iter->act_f_flg &&
				    ec_iter->act_t == ec_iter->act_f) {
					n_cnt = _db_tree_remove(
							&(s_iter->chains),
							ec_iter);
					s_iter->node_cnt -= n_cnt;
					goto add_free_ok;
				}
			} else if (db_chain_action(c_iter)) {
				/* new is shorter */
				if (c_iter->act_t_flg) {
					rc = _db_tree_act_check(ec_iter->nxt_t,
								action);
					if (rc < 0)
						goto add_free;
					n_cnt = _db_tree_free(ec_iter->nxt_t);
					ec_iter->nxt_t = NULL;
					ec_iter->act_t_flg = true;
					ec_iter->act_t = action;
				} else {
					rc = _db_tree_act_check(ec_iter->nxt_f,
								action);
					if (rc < 0)
						goto add_free;
					n_cnt = _db_tree_free(ec_iter->nxt_f);
					ec_iter->nxt_f = NULL;
					ec_iter->act_f_flg = true;
					ec_iter->act_f = action;
				}
				s_iter->node_cnt -= n_cnt;
			}
			if (c_iter->nxt_t != NULL) {
				if (ec_iter->nxt_t != NULL) {
					/* jump to the next level */
					c_prev = c_iter;
					c_iter = c_iter->nxt_t;
					ec_iter = ec_iter->nxt_t;
					s_new->node_cnt--;
				} else if (ec_iter->act_t_flg) {
					/* existing is shorter */
					if (ec_iter->act_t == action)
						goto add_free_ok;
					goto add_free_exist;
				} else {
					/* add a new branch */
					c_prev = c_iter;
					ec_iter->nxt_t = c_iter->nxt_t;
					s_iter->node_cnt +=
						(s_new->node_cnt - 1);
					goto add_free_match;
				}
			} else if (c_iter->nxt_f != NULL) {
				if (ec_iter->nxt_f != NULL) {
					/* jump to the next level */
					c_prev = c_iter;
					c_iter = c_iter->nxt_f;
					ec_iter = ec_iter->nxt_f;
					s_new->node_cnt--;
				} else if (ec_iter->act_f_flg) {
					/* existing is shorter */
					if (ec_iter->act_f == action)
						goto add_free_ok;
					goto add_free_exist;
				} else {
					/* add a new branch */
					c_prev = c_iter;
					ec_iter->nxt_f = c_iter->nxt_f;
					s_iter->node_cnt +=
						(s_new->node_cnt - 1);
					goto add_free_match;
				}
			} else
				goto add_free_ok;
		} else {
			/* need to check other nodes on this level */
			if (db_chain_lt(c_iter, ec_iter)) {
				if (ec_iter->lvl_prv == NULL) {
					/* add to the start of the level */
					ec_iter->lvl_prv = c_iter;
					c_iter->lvl_nxt = ec_iter;
					if (ec_iter == s_iter->chains)
						s_iter->chains = c_iter;
					s_iter->node_cnt += s_new->node_cnt;
					goto add_free_match;
				} else
					ec_iter = ec_iter->lvl_prv;
			} else {
				if (ec_iter->lvl_nxt == NULL) {
					/* add to the end of the level */
					ec_iter->lvl_nxt = c_iter;
					c_iter->lvl_prv = ec_iter;
					s_iter->node_cnt += s_new->node_cnt;
					goto add_free_match;
				} else if (db_chain_lt(c_iter,
						       ec_iter->lvl_nxt)) {
					/* add new chain in between */
					c_iter->lvl_nxt = ec_iter->lvl_nxt;
					ec_iter->lvl_nxt->lvl_prv = c_iter;
					ec_iter->lvl_nxt = c_iter;
					c_iter->lvl_prv = ec_iter;
					s_iter->node_cnt += s_new->node_cnt;
					goto add_free_match;
				} else
					ec_iter = ec_iter->lvl_nxt;
			}
		}
	} while ((c_iter != NULL) && (ec_iter != NULL));

	/* we should never be here! */
	return -EFAULT;

add_free_exist:
	rc = -EEXIST;
	goto add_free;
add_free_ok:
	rc = 0;
add_free:
	/* free the new chain and its syscall struct */
	_db_tree_free(s_new->chains);
	free(s_new);
	goto add_priority_update;
add_free_match:
	/* free the matching portion of new chain */
	if (c_prev != NULL) {
		c_prev->nxt_t = NULL;
		c_prev->nxt_f = NULL;
		_db_tree_free(s_new->chains);
	}
	free(s_new);
	rc = 0;
add_priority_update:
	/* update the priority */
	if (s_iter != NULL) {
		s_iter->priority &= (~_DB_PRI_MASK_CHAIN);
		s_iter->priority |= (_DB_PRI_MASK_CHAIN - s_iter->node_cnt);
	}
	return rc;
}

/**
 * Set the priority of a given syscall
 * @param col the filter collection
 * @param syscall the syscall number
 * @param priority priority value, higher value == higher priority
 *
 * This function sets the priority of the given syscall; this value is used
 * when generating the seccomp filter code such that higher priority syscalls
 * will incur less filter code overhead than the lower priority syscalls in the
 * filter.  Returns zero on success, negative values on failure.
 *
 */
int db_col_syscall_priority(struct db_filter_col *col,
			    int syscall, uint8_t priority)
{
	int rc = 0, rc_tmp;
	unsigned int iter;
	int sc_tmp;
	struct db_filter *filter;

	for (iter = 0; iter < col->filter_cnt; iter++) {
		filter = col->filters[iter];
		sc_tmp = syscall;

		rc_tmp = arch_syscall_translate(filter->arch, &sc_tmp);
		if (rc_tmp < 0)
			goto priority_failure;

		/* if this is a pseudo syscall (syscall < 0) then we need to
		 * rewrite the syscall for some arch specific reason */
		if (sc_tmp < 0) {
			/* we set this as a strict op - we don't really care
			 * since priorities are a "best effort" thing - as we
			 * want to catch the -EDOM error and bail on this
			 * architecture */
			rc_tmp = arch_syscall_rewrite(filter->arch, &sc_tmp);
			if (rc_tmp == -EDOM)
				continue;
			if (rc_tmp < 0)
				goto priority_failure;
		}

		rc_tmp = _db_syscall_priority(filter, sc_tmp, priority);

priority_failure:
		if (rc == 0 && rc_tmp < 0)
			rc = rc_tmp;
	}

	return rc;
}

/**
 * Add a new rule to the current filter
 * @param col the filter collection
 * @param strict the strict flag
 * @param action the filter action
 * @param syscall the syscall number
 * @param arg_cnt the number of argument filters in the argument filter chain
 * @param arg_array the argument filter chain, (uint, enum scmp_compare, ulong)
 *
 * This function adds a new argument/comparison/value to the seccomp filter for
 * a syscall; multiple arguments can be specified and they will be chained
 * together (essentially AND'd together) in the filter.  When the strict flag
 * is true the function will fail if the exact rule can not be added to the
 * filter, if the strict flag is false the function will not fail if the
 * function needs to adjust the rule due to architecture specifics.  Returns
 * zero on success, negative values on failure.
 *
 */
int db_col_rule_add(struct db_filter_col *col,
		    bool strict, uint32_t action, int syscall,
		    unsigned int arg_cnt, const struct scmp_arg_cmp *arg_array)
{
	int rc = 0, rc_tmp;
	unsigned int iter;
	unsigned int chain_len;
	unsigned int arg_num;
	size_t chain_size;
	struct db_api_arg *chain = NULL;
	struct scmp_arg_cmp arg_data;
	int arg_position = 0;

	/* collect the arguments for the filter rule */
	chain_len = ARG_COUNT_MAX;
	chain_size = sizeof(*chain) * chain_len;
	chain = malloc(chain_size);
	if (chain == NULL)
		return -ENOMEM;
	memset(chain, 0, chain_size);
	for (iter = 0; iter < arg_cnt; iter++) {
		arg_data = arg_array[iter];
		arg_num = arg_data.arg;
		if (arg_num < chain_len && chain[arg_num].valid == 0) {
			chain[arg_num].valid = 1;
			chain[arg_num].arg = arg_num;
			chain[arg_num].op = arg_data.op;
			/* XXX - we should check datum/mask size against the
			 *	 arch definition, e.g. 64 bit datum on x86 */
			switch (chain[arg_num].op) {
			case SCMP_CMP_NE:
			case SCMP_CMP_LT:
			case SCMP_CMP_LE:
			case SCMP_CMP_EQ:
			case SCMP_CMP_GE:
			case SCMP_CMP_GT:
				chain[arg_num].mask = DATUM_MAX;
				chain[arg_num].datum = arg_data.datum_a;
				arg_position += (1 << arg_num);
				break;
			case SCMP_CMP_MASKED_EQ:
				chain[arg_num].mask = arg_data.datum_a;
				chain[arg_num].datum = arg_data.datum_b;
				break;
			default:
				rc = -EINVAL;
				goto add_return;
			}
		} else {
			rc = -EINVAL;
			goto add_return;
		}
	}

	for (iter = 0; iter < col->filter_cnt; iter++) {
		rc_tmp = arch_filter_rule_add(col, col->filters[iter], strict,
					      action, syscall,
					      chain_len, chain);
		if (rc == 0 && rc_tmp < 0)
			rc = rc_tmp;
	}
	
	sys_draco_add(syscall, arg_position);

add_return:
	if (chain != NULL)
		free(chain);
	return rc;
}

/**
 * Start a new seccomp filter transaction
 * @param col the filter collection
 *
 * This function starts a new seccomp filter transaction for the given filter
 * collection.  Returns zero on success, negative values on failure.
 *
 */
int db_col_transaction_start(struct db_filter_col *col)
{
	unsigned int iter;
	size_t args_size;
	struct db_filter_snap *snap;
	struct db_filter *filter_o, *filter_s;
	struct db_api_rule_list *rule_o, *rule_s;

	/* allocate the snapshot */
	snap = malloc(sizeof(*snap));
	if (snap == NULL)
		return -ENOMEM;
	snap->filters = malloc(sizeof(struct db_filter *) * col->filter_cnt);
	if (snap->filters == NULL) {
		free(snap);
		return -ENOMEM;
	}
	snap->filter_cnt = col->filter_cnt;
	for (iter = 0; iter < snap->filter_cnt; iter++)
		snap->filters[iter] = NULL;
	snap->next = NULL;

	/* create a snapshot of the current filter state */
	for (iter = 0; iter < col->filter_cnt; iter++) {
		/* allocate a new filter */
		filter_o = col->filters[iter];
		filter_s = _db_init(filter_o->arch);
		if (filter_s == NULL)
			goto trans_start_failure;
		snap->filters[iter] = filter_s;

		/* create a filter snapshot from existing rules */
		rule_o = filter_o->rules;
		if (rule_o == NULL)
			continue;
		do {
			/* copy the rule */
			rule_s = malloc(sizeof(*rule_s));
			if (rule_s == NULL)
				goto trans_start_failure;
			args_size = sizeof(*rule_s->args) * rule_o->args_cnt;
			rule_s->args = malloc(args_size);
			if (rule_s->args == NULL) {
				free(rule_s);
				goto trans_start_failure;
			}
			rule_s->action = rule_o->action;
			rule_s->syscall = rule_o->syscall;
			rule_s->args_cnt = rule_o->args_cnt;
			memcpy(rule_s->args, rule_o->args, args_size);
			if (filter_s->rules != NULL) {
				rule_s->prev = filter_s->rules->prev;
				rule_s->next = filter_s->rules;
				filter_s->rules->prev->next = rule_s;
				filter_s->rules->prev = rule_s;
			} else {
				rule_s->prev = rule_s;
				rule_s->next = rule_s;
				filter_s->rules = rule_s;
			}

			/* insert the rule into the filter */
			if (db_rule_add(filter_s, rule_o) != 0)
				goto trans_start_failure;

			/* next rule */
			rule_o = rule_o->next;
		} while (rule_o != filter_o->rules);
	}

	/* add the snapshot to the list */
	snap->next = col->snapshots;
	col->snapshots = snap;

	return 0;

trans_start_failure:
	_db_snap_release(snap);
	return -ENOMEM;
}

/**
 * Abort the top most seccomp filter transaction
 * @param col the filter collection
 *
 * This function aborts the most recent seccomp filter transaction.
 *
 */
void db_col_transaction_abort(struct db_filter_col *col)
{
	int iter;
	unsigned int filter_cnt;
	struct db_filter **filters;
	struct db_filter_snap *snap;

	if (col->snapshots == NULL)
		return;

	/* replace the current filter with the last snapshot */
	snap = col->snapshots;
	col->snapshots = snap->next;
	filter_cnt = col->filter_cnt;
	filters = col->filters;
	col->filter_cnt = snap->filter_cnt;
	col->filters = snap->filters;
	free(snap);

	/* free the filter we swapped out */
	for (iter = 0; iter < filter_cnt; iter++)
		_db_release(filters[iter]);
	free(filters);
}

/**
 * Commit the top most seccomp filter transaction
 * @param col the filter collection
 *
 * This function commits the most recent seccomp filter transaction.
 *
 */
void db_col_transaction_commit(struct db_filter_col *col)
{
	struct db_filter_snap *snap;

	snap = col->snapshots;
	col->snapshots = snap->next;
	_db_snap_release(snap);
}
