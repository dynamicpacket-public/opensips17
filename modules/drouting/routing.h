/*
 * $Id$
 *
 * Copyright (C) 2005-2008 Voice Sistem SRL
 *
 * This file is part of Open SIP Server (OpenSIPS).
 *
 * DROUTING OpenSIPS-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * DROUTING OpenSIPS-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-system.ro
 *
 * History:
 * ---------
 *  2005-02-20  first version (cristian)
 *  2005-02-27  ported to 0.9.0 (bogdan)
 */


#ifndef routing_h
#define routing_h

#include "../../str.h"
#include "../../usr_avp.h"
#include "../../time_rec.h"
#include "prefix_tree.h"

#define RG_HASH_SIZE
#define RG_INIT_LEN 4;

/* the buckets for the rt_data rg_hash */
typedef struct hb_ {
	int rgid;
	ptree_t *pt;
	struct hb_*next;
} hb_t;

/* routing data is comprised of:
	- a list of PSTN gw
	- a hash over routing groups containing 
	pointers to the coresponding prefix trees
*/
typedef struct rt_data_ {
	/* list of PSTN gw */
	pgw_t *pgw_l;
	/* default routing list for prefixless rules */
	ptree_node_t noprefix;
	/* hash table with routing prefixes */
	ptree_t *pt;
}rt_data_t;

typedef struct _dr_group {
	/* 0 - use grp ; 1 - use AVP */
	int type;
	union {
		unsigned int grp_id;
		struct _avp_id{
			int name;
			unsigned short type;
		}avp_id;
	}u;
} dr_group_t;

/* init new rt_data structure */
rt_data_t*
build_rt_data( void );


/* add a PSTN gw in the list */
int
add_dst(
	rt_data_t*,
	/* id */
	int ,
	/* ip address */ 
	char*,
	/* strip len */
	int,
	/* pri prefix */
	char*,
	/* dst type*/
	int,
	/* dst attrs*/
	char*,
	/* probe_mode */
	int
	);

/* build a routing info list element */
rt_info_t*
build_rt_info(
	int id,
	int priority,
	tmrec_t* time,
	/* ser routing table id */
	int route_id,
	/* list of destinations indexes */
	char* dstlst,
	char* attr,
	pgw_t* pgw_l
);

void
del_pgw_list(
		pgw_t *pgw_l
		);

void 
free_rt_data(
		rt_data_t*,
		int
		);
#endif
