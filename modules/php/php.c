/*
 * $Id$
 *
 * PHP module for OpenSIPS
 *
 * Copyright (C) 2011 Dynamic Packet
 *                    (DEV TEAM <git@dynamicpacket.net>)
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define DEFAULTMODULE "OpenSIPS"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mi/mi.h"
#include "../rr/api.h"
#include "../sl/sl_api.h"

#include "php.h"

/* lock_ops.h defines union semun, perl does not need to redefine it */
#ifdef USE_SYSV_SEM
# define HAS_UNION_SEMUN
#endif

/* Full path to the script including executed functions */
char *filename = NULL;

/* Path to an arbitrary directory where the OpenSIPS PHP modules are
 * installed */
char *modpath = NULL;

/* Allow unsafe module functions - functions with fixups. This will create
 * memory leaks, the variable thus is not documented! */
int unsafemodfnc = 0;

/** SIGNALING binds */
struct sig_binds sigb;

/*
 * Module destroy function prototype
 */
static void destroy(void);

/*
 * Module child-init function prototype
 */
static int child_init(int rank);

/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Reload perl interpreter - reload perl script. Forward declaration.
 */
struct mi_root* php_mi_reload(struct mi_root *cmd_tree, void *param);



/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{ "php_exec_simple", (cmd_function)php_exec_simple1, 1,  NULL, 0,
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
	{ "php_exec_simple", (cmd_function)php_exec_simple2, 2,  NULL, 0,
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
/*
	{ "php_exec", (cmd_function)php_exec1, 1,  NULL, 0, 
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
	{ "php_exec", (cmd_function)php_exec2, 2, NULL, 0,
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
*/
	{ 0, 0, 0, 0, 0, 0 }
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"filename", STR_PARAM, &filename},
	{"modpath", STR_PARAM, &modpath},
	{"unsafemodfnc", INT_PARAM, &unsafemodfnc},
	{ 0, 0, 0 }
};


/*
 * Exported MI functions
 */
static mi_export_t mi_cmds[] = {
	/* FIXME This does not yet work... 
	{ "perl_reload",  php_mi_reload, MI_NO_INPUT_FLAG,  0,  0  },*/
	{ 0, 0, 0, 0, 0}

};




/*
 * Module info
 */

#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif

#ifndef RTLD_GLOBAL
/* Unsupported! */
#define RTLD_GLOBAL 0
#endif

/*
 * Module interface
 */
struct module_exports exports = {
	"php", 
	MODULE_VERSION,
	RTLD_NOW | RTLD_GLOBAL,
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	return 0;
}


/*
 * Reinit through fifo.
 * Currently does not seem to work :((
 */
struct mi_root* php_mi_reload(struct mi_root *cmd_tree, void *param)
{
	if (1) {
		return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	} else {
		return init_mi_tree( 500, "PHP reload failed", 18);
	}

}


/*
 * mod_init
 * Called by opensips at init time
 */
static int mod_init(void) {

	int ret = 0;

	LM_INFO("initializing...\n");

	if (!filename) {
		LM_ERR("insufficient module parameters. Module not loaded.\n");
		return -1;
	}

	/**
	 * We will need reply() from signaling
	 * module for sending replies
	 */

	/* load SIGNALING API */
	if(load_sig_api(&sigb)< 0) {
		LM_ERR("can't load signaling functions\n");
		return -1;
	}

	PERL_SYS_INIT3(NULL, NULL, &environ);

	if ((os_php_init())) {

		ret = 0;

	} else {
		ret = -1;
	}

	return ret;
}

/*
 * destroy
 * called by opensips at exit time
 */
static void destroy(void)
{
	os_php_unload();
}

