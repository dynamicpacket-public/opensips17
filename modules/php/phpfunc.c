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


#include <string.h>
#include <stdio.h>

#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/parse_param.h"
#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "../../action.h"
#include "../../config.h"
#include "../../parser/parse_uri.h"

#include "phpfunc.h"
#include "php.h"


/*
 * Check for existence of a function.
 */
int php_checkfnc(char *fnc) {

	return 1;
}

/*
 * Run function without paramters
 */

int php_exec_simple(char* fnc, char* args[], int flags) {

	if (php_checkfnc(fnc)) {
		LM_DBG("running php function \"%s\"", fnc);

		// call_argv(fnc, flags, args);
	} else {
		LM_ERR("unknown function '%s' called.\n", fnc);
		return -1;
	}

	return 1;
}

int php_exec_simple1(struct sip_msg* _msg, char* fnc, char* str2) {
	char *args[] = { NULL };

	return php_exec_simple(fnc, args, 1);
}

int php_exec_simple2(struct sip_msg* _msg, char* fnc, char* param) {
	char *args[] = { param, NULL };

	return php_exec_simple(fnc, args, 1);
}