/* 
 * $Id$ 
 *
 * Usrloc contact structure
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * History:
 * ---------
 * 2003-03-12 added replication mark and three zombie states (nils)
 * 2004-03-17 generic callbacks added (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 */

/*! \file
 *  \brief USRLOC - Usrloc contact structure
 *  \ingroup usrloc
 */

#include "ucontact.h"
#include <string.h>             /* memcpy */
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "../../db/db_insertq.h"
#include "ul_mod.h"
#include "ul_callback.h"
#include "urecord.h"
#include "ucontact.h"


/*! \brief
 * Create a new contact structure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact, ucontact_info_t* _ci)
{
	ucontact_t *c;

	c = (ucontact_t*)shm_malloc(sizeof(ucontact_t));
	if (!c) {
		LM_ERR("no more shm memory\n");
		return 0;
	}
	memset(c, 0, sizeof(ucontact_t));

	if (shm_str_dup( &c->c, _contact) < 0) goto error;
	if (shm_str_dup( &c->callid, _ci->callid) < 0) goto error;
	if (shm_str_dup( &c->user_agent, _ci->user_agent) < 0) goto error;

	if (_ci->received.s && _ci->received.len) {
		if (shm_str_dup( &c->received, &_ci->received) < 0) goto error;
	}
	if (_ci->path && _ci->path->len) {
		if (shm_str_dup( &c->path, _ci->path) < 0) goto error;
	}

	c->domain = _dom;
	c->aor = _aor;
	c->expires = _ci->expires;
	c->q = _ci->q;
	c->sock = _ci->sock;
	c->cseq = _ci->cseq;
	c->state = CS_NEW;
	c->flags = _ci->flags;
	c->cflags = _ci->cflags;
	c->methods = _ci->methods;
	c->last_modified = _ci->last_modified;

	return c;
error:
	LM_ERR("no more shm memory\n");
	if (c->path.s) shm_free(c->path.s);
	if (c->received.s) shm_free(c->received.s);
	if (c->user_agent.s) shm_free(c->user_agent.s);
	if (c->callid.s) shm_free(c->callid.s);
	if (c->c.s) shm_free(c->c.s);
	shm_free(c);
	return 0;
}



/*! \brief
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c)
{
	if (!_c) return;
	if (_c->path.s) shm_free(_c->path.s);
	if (_c->received.s) shm_free(_c->received.s);
	if (_c->user_agent.s) shm_free(_c->user_agent.s);
	if (_c->callid.s) shm_free(_c->callid.s);
	if (_c->c.s) shm_free(_c->c.s);
	shm_free( _c );
}


/*! \brief
 * Print contact, for debugging purposes only
 */
void print_ucontact(FILE* _f, ucontact_t* _c)
{
	time_t t = time(0);
	char* st;

	switch(_c->state) {
	case CS_NEW:   st = "CS_NEW";     break;
	case CS_SYNC:  st = "CS_SYNC";    break;
	case CS_DIRTY: st = "CS_DIRTY";   break;
	default:       st = "CS_UNKNOWN"; break;
	}

	fprintf(_f, "~~~Contact(%p)~~~\n", _c);
	fprintf(_f, "domain    : '%.*s'\n", _c->domain->len, ZSW(_c->domain->s));
	fprintf(_f, "aor       : '%.*s'\n", _c->aor->len, ZSW(_c->aor->s));
	fprintf(_f, "Contact   : '%.*s'\n", _c->c.len, ZSW(_c->c.s));
	fprintf(_f, "Expires   : ");
	if (_c->expires == 0) {
		fprintf(_f, "Permanent\n");
	} else if (_c->expires == UL_EXPIRED_TIME) {
		fprintf(_f, "Deleted\n");
	} else if (t > _c->expires) {
		fprintf(_f, "Expired\n");
	} else {
		fprintf(_f, "%u\n", (unsigned int)(_c->expires - t));
	}
	fprintf(_f, "q         : %s\n", q2str(_c->q, 0));
	fprintf(_f, "Call-ID   : '%.*s'\n", _c->callid.len, ZSW(_c->callid.s));
	fprintf(_f, "CSeq      : %d\n", _c->cseq);
	fprintf(_f, "User-Agent: '%.*s'\n",
		_c->user_agent.len, ZSW(_c->user_agent.s));
	fprintf(_f, "received  : '%.*s'\n",
		_c->received.len, ZSW(_c->received.s));
	fprintf(_f, "Path      : '%.*s'\n",
		_c->path.len, ZSW(_c->path.s));
	fprintf(_f, "State     : %s\n", st);
	fprintf(_f, "Flags     : %u\n", _c->flags);
	if (_c->sock) {
		fprintf(_f, "Sock      : %.*s (as %.*s )(%p)\n",
				_c->sock->sock_str.len,_c->sock->sock_str.s,
				_c->sock->adv_sock_str.len,ZSW(_c->sock->adv_sock_str.s),
				_c->sock);
	} else {
		fprintf(_f, "Sock      : none (null)\n");
	}
	fprintf(_f, "Methods   : %u\n", _c->methods);
	fprintf(_f, "next      : %p\n", _c->next);
	fprintf(_f, "prev      : %p\n", _c->prev);
	fprintf(_f, "~~~/Contact~~~~\n");
}


/*! \brief
 * Update ucontact structure in memory
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t* _ci)
{
#define update_str(_old,_new) \
	do{\
		if ((_old)->len < (_new)->len) { \
			ptr = (char*)shm_malloc((_new)->len); \
			if (ptr == 0) { \
				LM_ERR("no more shm memory\n"); \
				return -1; \
			}\
			memcpy(ptr, (_new)->s, (_new)->len);\
			if ((_old)->s) shm_free((_old)->s);\
			(_old)->s = ptr;\
		} else {\
			memcpy((_old)->s, (_new)->s, (_new)->len);\
		}\
		(_old)->len = (_new)->len;\
	} while(0)

	char* ptr;

	/* No need to update Callid as it is constant 
	 * per ucontact (set at insert time)  -bogdan */

	update_str( &_c->user_agent, _ci->user_agent);

	if (_ci->received.s && _ci->received.len) {
		update_str( &_c->received, &_ci->received);
	} else {
		if (_c->received.s) shm_free(_c->received.s);
		_c->received.s = 0;
		_c->received.len = 0;
	}
	
	if (_ci->path) {
		update_str( &_c->path, _ci->path);
	} else {
		if (_c->path.s) shm_free(_c->path.s);
		_c->path.s = 0;
		_c->path.len = 0;
	}

	_c->sock = _ci->sock;
	_c->expires = _ci->expires;
	_c->q = _ci->q;
	_c->cseq = _ci->cseq;
	_c->methods = _ci->methods;
	_c->last_modified = _ci->last_modified;
	_c->flags = _ci->flags;
	_c->cflags = _ci->cflags;

	return 0;
}


/* ================ State related functions =============== */


/*! \brief
 * Update state of the contact
 */
void st_update_ucontact(ucontact_t* _c)
{
	switch(_c->state) {
	case CS_NEW:
			 /* Contact is new and is not in the database yet,
			  * we remain in the same state here because the
			  * contact must be inserted later in the timer
			  */
		break;

	case CS_SYNC:
			 /* For db mode 1 & 2 a modified contact needs to be 
			  * updated also in the database, so transit into 
			  * CS_DIRTY and let the timer to do the update 
			  * again. For db mode 1 we try to update right
			  * now and if fails, let the timer to do the job
			  */
		if (db_mode == WRITE_BACK || db_mode == WRITE_THROUGH) {
			_c->state = CS_DIRTY;
		}
		break;

	case CS_DIRTY:
			 /* Modification of dirty contact results in
			  * dirty contact again, don't change anything
			  */
		break;
	}
}


/*! \brief
 * Update state of the contact
 * \return 1 if the contact should be deleted from memory immediately,
 * 0 otherwise
 */
int st_delete_ucontact(ucontact_t* _c)
{
	switch(_c->state) {
	case CS_NEW:
		     /* Contact is new and isn't in the database
		      * yet, we can delete it from the memory
		      * safely.
		      */
		return 1;

	case CS_SYNC:
	case CS_DIRTY:
		     /* Contact is in the database,
		      * we cannot remove it from the memory 
		      * directly, but we can set expires to zero
		      * and the timer will take care of deleting 
		      * the contact from the memory as well as 
		      * from the database
		      */
		if (db_mode == WRITE_BACK) {
			_c->expires = UL_EXPIRED_TIME;
			return 0;
		} else {
			     /* WRITE_THROUGH or NO_DB -- we can
			      * remove it from memory immediately and
			      * the calling function would also remove
			      * it from the database if needed
			      */
			return 1;
		}
	}

	return 0; /* Makes gcc happy */
}


/*! \brief
 * Called when the timer is about to delete
 * an expired contact. 
 * \return 1 if the contact should be removed from
 * the database and 0 otherwise
 */
int st_expired_ucontact(ucontact_t* _c)
{
	     /* There is no need to change contact
	      * state, because the contact will
	      * be deleted anyway
	      */

	switch(_c->state) {
	case CS_NEW:
		     /* Contact is not in the database
		      * yet, remove it from memory only
		      */
		return 0;

	case CS_SYNC:
	case CS_DIRTY:
		     /* Remove from database here */
		return 1;
	}

	return 0; /* Makes gcc happy */
}


/*! \brief
 * Called when the timer is about flushing the contact,
 * updates contact state and returns 1 if the contact
 * should be inserted, 2 if update and 0 otherwise
 */
int st_flush_ucontact(ucontact_t* _c)
{
	switch(_c->state) {
	case CS_NEW:
		     /* Contact is new and is not in
		      * the database yet so we have
		      * to insert it
		      */
		_c->state = CS_SYNC;
		return 1;

	case CS_SYNC:
		     /* Contact is synchronized, do
		      * nothing
		      */
		return 0;

	case CS_DIRTY:
		     /* Contact has been modified and
		      * is in the db already so we
		      * have to update it
		      */
		_c->state = CS_SYNC;
		return 2;
	}

	return 0; /* Makes gcc happy */
}


/* ============== Database related functions ================ */

/*! \brief
 * Insert contact into the database
 */
int db_insert_ucontact(ucontact_t* _c,query_list_t **ins_list, int update)
{
	static db_ps_t myI_ps = NULL;
	static db_ps_t myR_ps = NULL;
	char* dom;
	db_key_t keys[15];
	db_val_t vals[15];

	if (_c->flags & FL_MEM) {
		return 0;
	}

	keys[0] = &user_col;
	keys[1] = &contact_col;
	keys[2] = &expires_col;
	keys[3] = &q_col;
	keys[4] = &callid_col;
	keys[5] = &cseq_col;
	keys[6] = &flags_col;
	keys[7] = &cflags_col;
	keys[8] = &user_agent_col;
	keys[9] = &received_col;
	keys[10] = &path_col;
	keys[11] = &sock_col;
	keys[12] = &methods_col;
	keys[13] = &last_mod_col;
	keys[14] = &domain_col;

	vals[0].type = DB_STR;
	vals[0].nul = 0;
	vals[0].val.str_val.s = _c->aor->s;
	vals[0].val.str_val.len = _c->aor->len;

	vals[1].type = DB_STR;
	vals[1].nul = 0;
	vals[1].val.str_val.s = _c->c.s; 
	vals[1].val.str_val.len = _c->c.len;

	vals[2].type = DB_DATETIME;
	vals[2].nul = 0;
	vals[2].val.time_val = _c->expires;

	vals[3].type = DB_DOUBLE;
	vals[3].nul = 0;
	vals[3].val.double_val = q2double(_c->q);

	vals[4].type = DB_STR;
	vals[4].nul = 0;
	vals[4].val.str_val.s = _c->callid.s;
	vals[4].val.str_val.len = _c->callid.len;

	vals[5].type = DB_INT;
	vals[5].nul = 0;
	vals[5].val.int_val = _c->cseq;

	vals[6].type = DB_INT;
	vals[6].nul = 0;
	vals[6].val.bitmap_val = _c->flags;

	vals[7].type = DB_INT;
	vals[7].nul = 0;
	vals[7].val.bitmap_val = _c->cflags;

	vals[8].type = DB_STR;
	vals[8].nul = 0;
	vals[8].val.str_val.s = _c->user_agent.s;
	vals[8].val.str_val.len = _c->user_agent.len;

	vals[9].type = DB_STR;
	if (_c->received.s == 0) {
		vals[9].nul = 1;
	} else {
		vals[9].nul = 0;
		vals[9].val.str_val.s = _c->received.s;
		vals[9].val.str_val.len = _c->received.len;
	}
	
	vals[10].type = DB_STR;
	if (_c->path.s == 0) {
		vals[10].nul = 1;
	} else {
		vals[10].nul = 0;
		vals[10].val.str_val.s = _c->path.s;
		vals[10].val.str_val.len = _c->path.len;
	}

	vals[11].type = DB_STR;
	if (_c->sock) {
		vals[11].val.str_val =  _c->sock->adv_sock_str.len ?
								_c->sock->adv_sock_str:  _c->sock->sock_str;
		vals[11].nul = 0;
	} else {
		vals[11].nul = 1;
	}

	vals[12].type = DB_BITMAP;
	if (_c->methods == 0xFFFFFFFF) {
		vals[12].nul = 1;
	} else {
		vals[12].val.bitmap_val = _c->methods;
		vals[12].nul = 0;
	}

	vals[13].type = DB_DATETIME;
	vals[13].nul = 0;
	vals[13].val.time_val = _c->last_modified;

	if (use_domain) {
		vals[14].type = DB_STR;
		vals[14].nul = 0;

		dom = q_memchr(_c->aor->s, '@', _c->aor->len);
		if (dom==0) {
			vals[0].val.str_val.len = 0;
			vals[14].val.str_val = *_c->aor;
		} else {
			vals[0].val.str_val.len = dom - _c->aor->s;
			vals[14].val.str_val.s = dom + 1;
			vals[14].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
		}
	}

	if (ul_dbf.use_table(ul_dbh, _c->domain) < 0) {
		LM_ERR("sql use_table failed\n");
		return -1;
	}


	if ( !update ) {
		/* do simple insert */
		CON_PS_REFERENCE(ul_dbh) = &myI_ps;
		if (ins_list) {
			if (con_set_inslist(&ul_dbf,ul_dbh,ins_list,keys,
						(use_domain) ? (15) : (14)) < 0 )
				CON_RESET_INSLIST(ul_dbh);
		}

		if (ul_dbf.insert(ul_dbh, keys, vals, (use_domain) ? (15) : (14)) < 0) {
			LM_ERR("inserting contact in db failed\n");
			return -1;
		}
	} else {
		/* do insert-update / replace */
		CON_PS_REFERENCE(ul_dbh) = &myR_ps;
		if (ul_dbf.insert_update(ul_dbh, keys, vals, (use_domain) ? (15) : (14)) < 0) {
			LM_ERR("inserting contact in db failed\n");
			return -1;
		}
	}

	return 0;
}


/*! \brief
 * Update contact in the database
 */
int db_update_ucontact(ucontact_t* _c)
{
	static db_ps_t my_ps = NULL;
	char* dom;
	db_key_t keys1[4];
	db_val_t vals1[4];

	db_key_t keys2[11];
	db_val_t vals2[11];

	if (_c->flags & FL_MEM) {
		return 0;
	}

	keys1[0] = &user_col;
	keys1[1] = &contact_col;
	keys1[2] = &callid_col;
	keys1[3] = &domain_col;
	keys2[0] = &expires_col;
	keys2[1] = &q_col;
	keys2[2] = &cseq_col;
	keys2[3] = &flags_col;
	keys2[4] = &cflags_col;
	keys2[5] = &user_agent_col;
	keys2[6] = &received_col;
	keys2[7] = &path_col;
	keys2[8] = &sock_col;
	keys2[9] = &methods_col;
	keys2[10] = &last_mod_col;

	vals1[0].type = DB_STR;
	vals1[0].nul = 0;
	vals1[0].val.str_val = *_c->aor;

	vals1[1].type = DB_STR;
	vals1[1].nul = 0;
	vals1[1].val.str_val = _c->c;

	vals1[2].type = DB_STR;
	vals1[2].nul = 0;
	vals1[2].val.str_val = _c->callid;

	vals2[0].type = DB_DATETIME;
	vals2[0].nul = 0;
	vals2[0].val.time_val = _c->expires;

	vals2[1].type = DB_DOUBLE;
	vals2[1].nul = 0;
	vals2[1].val.double_val = q2double(_c->q);

	vals2[2].type = DB_INT;
	vals2[2].nul = 0;
	vals2[2].val.int_val = _c->cseq;

	vals2[3].type = DB_INT;
	vals2[3].nul = 0;
	vals2[3].val.bitmap_val = _c->flags;

	vals2[4].type = DB_INT;
	vals2[4].nul = 0;
	vals2[4].val.bitmap_val = _c->cflags;

	vals2[5].type = DB_STR;
	vals2[5].nul = 0;
	vals2[5].val.str_val = _c->user_agent;

	vals2[6].type = DB_STR;
	if (_c->received.s == 0) {
		vals2[6].nul = 1;
	} else {
		vals2[6].nul = 0;
		vals2[6].val.str_val = _c->received;
	}
	
	vals2[7].type = DB_STR;
	if (_c->path.s == 0) {
		vals2[7].nul = 1;
	} else {
		vals2[7].nul = 0;
		vals2[7].val.str_val = _c->path;
	}

	vals2[8].type = DB_STR;
	if (_c->sock) {
		vals2[8].val.str_val = _c->sock->adv_sock_str.len ?
								_c->sock->adv_sock_str:  _c->sock->sock_str;
		vals2[8].nul = 0;
	} else {
		vals2[8].nul = 1;
	}

	vals2[9].type = DB_BITMAP;
	if (_c->methods == 0xFFFFFFFF) {
		vals2[9].nul = 1;
	} else {
		vals2[9].val.bitmap_val = _c->methods;
		vals2[9].nul = 0;
	}

	vals2[10].type = DB_DATETIME;
	vals2[10].nul = 0;
	vals2[10].val.time_val = _c->last_modified;

	if (use_domain) {
		vals1[3].type = DB_STR;
		vals1[3].nul = 0;
		dom = q_memchr(_c->aor->s, '@', _c->aor->len);
		if (dom==0) {
			vals1[0].val.str_val.len = 0;
			vals1[3].val.str_val = *_c->aor;
		} else {
			vals1[0].val.str_val.len = dom - _c->aor->s;
			vals1[3].val.str_val.s = dom + 1;
			vals1[3].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
		}
	}

	if (ul_dbf.use_table(ul_dbh, _c->domain) < 0) {
		LM_ERR("sql use_table failed\n");
		return -1;
	}

	CON_PS_REFERENCE(ul_dbh) = &my_ps;

	if (ul_dbf.update(ul_dbh, keys1, 0, vals1, keys2, vals2, 
	(use_domain) ? (4) : (3), 11) < 0) {
		LM_ERR("updating database failed\n");
		return -1;
	}

	return 0;
}


/*! \brief
 * Delete contact from the database
 */
int db_delete_ucontact(ucontact_t* _c)
{
	static db_ps_t my_ps = NULL;
	char* dom;
	db_key_t keys[4];
	db_val_t vals[4];

	if (_c->flags & FL_MEM) {
		return 0;
	}

	keys[0] = &user_col;
	keys[1] = &contact_col;
	keys[2] = &callid_col;
	keys[3] = &domain_col;

	vals[0].type = DB_STR;
	vals[0].nul = 0;
	vals[0].val.str_val = *_c->aor;

	vals[1].type = DB_STR;
	vals[1].nul = 0;
	vals[1].val.str_val = _c->c;

	vals[2].type = DB_STR;
	vals[2].nul = 0;
	vals[2].val.str_val = _c->callid;

	if (use_domain) {
		vals[3].type = DB_STR;
		vals[3].nul = 0;
		dom = q_memchr(_c->aor->s, '@', _c->aor->len);
		if (dom==0) {
			vals[0].val.str_val.len = 0;
			vals[3].val.str_val = *_c->aor;
		} else {
			vals[0].val.str_val.len = dom - _c->aor->s;
			vals[3].val.str_val.s = dom + 1;
			vals[3].val.str_val.len = _c->aor->s + _c->aor->len - dom - 1;
		}
	}

	if (ul_dbf.use_table(ul_dbh, _c->domain) < 0) {
		LM_ERR("sql use_table failed\n");
		return -1;
	}

	CON_PS_REFERENCE(ul_dbh) = &my_ps;

	if (ul_dbf.delete(ul_dbh, keys, 0, vals, (use_domain) ? (4) : (3)) < 0) {
		LM_ERR("deleting from database failed\n");
		return -1;
	}

	return 0;
}



static inline void unlink_contact(struct urecord* _r, ucontact_t* _c)
{
	if (_c->prev) {
		_c->prev->next = _c->next;
		if (_c->next) {
			_c->next->prev = _c->prev;
		}
	} else {
		_r->contacts = _c->next;
		if (_c->next) {
			_c->next->prev = 0;
		}
	}
}



static inline void update_contact_pos(struct urecord* _r, ucontact_t* _c)
{
	ucontact_t *pos, *ppos;

	if (desc_time_order) {
		/* order by time - first the newest */
		if (_c->prev==0)
			return;
		unlink_contact(_r, _c);
		/* insert it at the beginning */
		_c->next = _r->contacts;
		_c->prev = 0;
		_r->contacts->prev = _c;
		_r->contacts = _c;
	} else {
		/* order by q - first the smaller q */
		if ( (_c->prev==0 || _c->q<=_c->prev->q)
		&& (_c->next==0 || _c->q>=_c->next->q)  )
			return;
		/* need to move , but where? */
		unlink_contact(_r, _c);
		_c->next = _c->prev = 0;
		for(pos=_r->contacts,ppos=0;pos&&pos->q<_c->q;ppos=pos,pos=pos->next);
		if (pos) {
			if (!pos->prev) {
				pos->prev = _c;
				_c->next = pos;
				_r->contacts = _c;
			} else {
				_c->next = pos;
				_c->prev = pos->prev;
				pos->prev->next = _c;
				pos->prev = _c;
			}
		} else if (ppos) {
			ppos->next = _c;
			_c->prev = ppos;
		} else {
			_r->contacts = _c;
		}
	}
}


/*! \brief
 * Update ucontact with new values
 */
int update_ucontact(struct urecord* _r, ucontact_t* _c, ucontact_info_t* _ci)
{
	int ret;

	/* we have to update memory in any case, but database directly
	 * only in db_mode 1 */
	if (mem_update_ucontact( _c, _ci) < 0) {
		LM_ERR("failed to update memory\n");
		return -1;
	}

	/* run callbacks for UPDATE event */
	if (exists_ulcb_type(UL_CONTACT_UPDATE))
	{
		LM_DBG("exists callback for type= UL_CONTACT_UPDATE\n");
		run_ul_callbacks( UL_CONTACT_UPDATE, _c);
	}

	if (_r && db_mode!=DB_ONLY)
		update_contact_pos( _r, _c);

	st_update_ucontact(_c);

	if (db_mode == WRITE_THROUGH || db_mode==DB_ONLY) {
		ret = (db_mode==DB_ONLY && DB_CAPABILITY(ul_dbf, DB_CAP_INSERT_UPDATE))?
			db_insert_ucontact(_c,NULL,1) : db_update_ucontact(_c) ;
		if (ret < 0) {
			LM_ERR("failed to update database\n");
		} else {
			_c->state = CS_SYNC;
		}
	}
	return 0;
}
