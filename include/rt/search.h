/*                        S E A R C H . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup db_search
 * @brief
 * Functionality for searching .g files
 */
/** @{ */
/** @file include/rt/search.h */

#ifndef RT_SEARCH_H
#define RT_SEARCH_H

#include "common.h"

#include "bu/list.h"
#include "bu/ptbl.h"

#include "rt/db_instance.h"
#include "rt/defines.h"

__BEGIN_DECLS

/**
 * @brief Search for objects in a geometry database using filters
 *
 * The db_search function is a programmatic find-style interface that
 * lets you search for objects in a geometry database.  This function
 * searches the database using a supplied list of filter criteria.
 *
 * The function returns a count of objects matching the filter
 * criteria and can provide the resulting matches in binary format as
 * either db_full_path or directory objects depending on the flags
 * (i.e., depending on whether this is a flat or hierarchical search).
 *
 * There are a LOT of filter possibilities.  See the search(n) manual
 * page for details.
 *
 * @param[out] results is a bu_ptbl holding either db_full_path or
 * directory pointers.
 *
 * @param flags is a bit field for setting various search options.
 *
 * @param filter is a string defining search filters to be used.
 *
 * @param path_c is the count of directory paths to be searched.
 *
 * @param path_v is one or more directory paths to be searched.  If
 * path_v itself is NULL, all top-level objects are searched
 *
 * @param dbip The database instance pointer corresponding to the
 * current geometry database.
 *
 * @param clbk Optional callback function to call for -exec
 *
 * @param u1 Optional user data pointer
 *
 * @param u2 Optional user data pointer
 *
 * @return Negative return values indicate a problem with the search,
 * and non-negative values indicate a successful search.  Non-negative
 * values correspond with the number of objects found.
 *
 * @retval -2 Return code when db_search is called with a NULL dbip.
 * @retval -1 Return code when the plan search string is invalid.
 * @retval 0  Return code when the search completed successfully but no matches were found.
 * @retval >0 Return code when the search completed successfully and matched one or more objects.
 *
 * The following example assumes a database instance pointer (dbip) is
 * available and ready to use.
 *
 * @code
 * size_t i = 0;
 * struct bu_ptbl results = BU_PTBL_INIT_ZERO;
 * const char *plan = "-name *.s -or -below -type region";
 * int matches = db_search(&results, DB_SEARCH_HIDDEN | DB_SEARCH_QUIET , plan, 0, NULL, dbip, ctx);
 * for (i = 0; matches > 0 && i < BU_PTBL_LEN(&results); i++) {
 *   char *path_str = db_path_to_string((struct db_full_path *)BU_PTBL_GET(&results, i));
 *   bu_log("%s\n", path_str);
 *   bu_free(path_str, "free db_fullpath_to_string allocation");
 * }
 * db_search_free(&results);
 * @endcode
 *
 * Note:
 * Be aware that if you are using db_search to filter pre-built lists of paths,
 * you need to check that your generated path list is NOT empty before calling
 * db_search.  If you accidentally send an empty path list into db_search,
 * it will assume you wanted a tops list, which has a good chance of returning
 * unwanted results.
 *
 */
RT_EXPORT extern int db_search(struct bu_ptbl *results,
			       int flags,
			       const char *filter,
			       int path_c,
			       struct directory **path_v,
			       struct db_i *dbip,
			       bu_clbk_t clbk,
			       void *u1,
			       void *u2
			      );

/* These are the possible search flags. */
#define DB_SEARCH_TREE             0x0   /**< @brief Do a hierarchy-aware search.  This is the default. */
#define DB_SEARCH_FLAT             0x1   /**< @brief Do a flat search without hierarchy */
#define DB_SEARCH_HIDDEN           0x2   /**< @brief Search using hidden objects */
#define DB_SEARCH_RETURN_UNIQ_DP   0x4   /**< @brief Return the set of unique directory pointers instead of full paths */
#define DB_SEARCH_QUIET            0x8   /**< @brief Silence all warnings */
#define DB_SEARCH_PRINT_TOTAL	   0x10	 /**< @brief Print total number of items found in search */

/**
 * Properly free the table contents returned by db_search.  The bu_ptbl
 * itself, if not put on the stack, will need to be freed by the same
 * calling function that allocated it.
 */
RT_EXPORT extern void db_search_free(struct bu_ptbl *search_results);


/* db_ls.c */
/**
 * db_ls takes a database instance pointer and assembles a directory
 * pointer array of objects in the database according to a set of
 * flags.  An optional pattern can be supplied for match filtering via
 * globbing rules (see bu_path_match()).  If pattern is NULL,
 * filtering is performed using only the flags.
 *
 * The caller is responsible for freeing the array.
 *
 * Returns -
 * integer count of objects in dpv
 * struct directory ** array of objects in dpv via argument
 *
 */
RT_EXPORT extern size_t db_ls(const struct db_i *dbip,
			      int flags,
			      const char *pattern,
			      struct directory ***dpv);

/* These are the possible listing flags. */
#define DB_LS_PRIM         0x1    /**< @brief filter for primitives (solids)*/
#define DB_LS_COMB         0x2    /**< @brief filter for combinations */
#define DB_LS_REGION       0x4    /**< @brief filter for regions */
#define DB_LS_HIDDEN       0x8    /**< @brief include hidden objects in results */
#define DB_LS_NON_GEOM     0x10   /**< @brief filter for non-geometry objects */
#define DB_LS_TOPS         0x20   /**< @brief filter for objects un-referenced by other objects */
#define DB_LS_CYCLIC       0x40   /**< @brief filter for objects with a cyclic reference in subtrees */
#define DB_LS_PHONY        0x80   /**< @brief enable and filter for objects such as the nirt display list entries */
/* TODO - implement this flag
   #define DB_LS_REGEX        0x100*/ /* interpret pattern using regex rules, instead of
					globbing rules (default) */

/* cyclic.c */
/**
 * db_cyclic_paths searches for cyclic paths in the database, either in all
 * objects or checking whether a specific dp is cyclic within its subtree.
 *
 * If sdp is NULL, ALL directory pointers in the database are checked.  This is
 * a complete validation of the whole .g file, and the only way to
 * comprehensively search for any cyclic paths present.  The return count will
 * be the number of combs with a cyclic reference in their subtrees.
 *
 * If sdp is non-NULL, the search will be limited to checking only the tree
 * below sdp for a cyclic reference to sdp.
 *
 * If a cyclic_paths is non-NULL it will be used to return db_fullpath entries
 * for the cyclic paths found.
 */
RT_EXPORT extern int db_cyclic_paths(struct bu_ptbl *cyclic_paths, const struct db_i *dbip, struct directory *sdp);


/* Deprecated */
typedef int(*db_search_callback_t)(int, const char*[],void*);
struct db_search_context {
    db_search_callback_t _e_callback; /**< @brief A function that evaluates an array of strings and returns a boolean. */
    void *_e_userdata; /**< @brief A pointer that will be passed to the callback, usually a pointer to an interpreter. */
};
RT_EXPORT extern struct db_search_context *db_search_context_create(void); /* FIXME: is this really needed? why not just use the struct directly from the stack or let the user handle allocation? */
RT_EXPORT extern void db_search_context_destroy(struct db_search_context *ctx);
RT_EXPORT extern void db_search_register_exec(struct db_search_context *, db_search_callback_t);
RT_EXPORT extern void db_search_register_data(struct db_search_context *, void *);
RT_EXPORT extern int db_search_old(struct bu_ptbl *results,
			       int flags,
			       const char *filter,
			       int path_c,
			       struct directory **path_v,
			       struct db_i *dbip,
			       struct db_search_context *ctx
			      );

__END_DECLS

#endif /* RT_SEARCH_H*/
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
