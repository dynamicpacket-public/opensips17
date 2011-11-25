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

#ifndef PHP_FUNC_H
#define PHP_FUNC_H

#include "../../parser/msg_parser.h"

/*
 * Run a php function without a sip message parameter.
 */
int php_exec_simple1(struct sip_msg* _msg, char* fnc, char* str2);
int php_exec_simple2(struct sip_msg* _msg, char* fnc, char* str2);

/*
 * Run function with a reference to the current SIP message.
 * An optional string may be passed to php_exec_string.
 */
/*
int php_exec1(struct sip_msg* _msg, char* fnc, char *foobar);
int php_exec2(struct sip_msg* _msg, char* fnc, char* mystr);
*/

#endif /* PHP_FUNC_H */