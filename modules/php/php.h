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

#ifndef PHP_MOD_H
#define PHP_MOD_H

#include "../signaling/signaling.h"

/* lock_ops.h defines union semun, perl does not need to redefine it */
#ifdef USE_SYSV_SEM
# define HAS_UNION_SEMUN
#endif

//#include <EXTERN.h>
#include <sapi/embed/php_embed.h>

extern char *filename;
extern char *modpath;

extern struct sig_binds sigb;

#include "phpfunc.h"

#endif /* PHP_MOD_H */
