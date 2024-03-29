/*
 * $Id$
 *
 * rls module - resource list server
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 * --------
 *  2007-09-11  initial version (Anca Vamanu)
 */

#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../pua/hash.h"
#include "rls.h"
#include "notify.h"
#include "resource_notify.h"

/* how to relate resource oriented dialogs to list_uri */
/* sol1: use the same callid in Subscribe requests 
 * sol2: include an extra header
 * sol3: put the list_uri as the id of the record stored in
 * pua and write a function to return that id
 * winner: sol3
 * */
static str su_200_rpl     = str_init("OK");

int parse_subs_state(str auth_state, str** reason, int* expires)
{
	str str_exp;
	str* res= NULL;
	char* smc= NULL;
	int len, flag= -1;


	if( strncasecmp(auth_state.s, "active", 6)== 0)
		flag= ACTIVE_STATE;

	if( strncasecmp(auth_state.s, "pending", 7)== 0)
		flag= PENDING_STATE; 

	if( strncasecmp(auth_state.s, "terminated", 10)== 0)
	{
		*expires = 0;
		smc= strchr(auth_state.s, ';');
		if(smc== NULL)
		{
			LM_ERR("terminated state and no reason found\n");
			return -1;
		}
		if(strncasecmp(smc+1, "reason=", 7))
		{
			LM_ERR("terminated state and no reason found\n");
			return -1;
		}
		res= (str*)pkg_malloc(sizeof(str));
		if(res== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		len=  auth_state.len- 10- 1- 7;
		res->s= (char*)pkg_malloc(len* sizeof(char));
		if(res->s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(res->s, smc+ 8, len);
		res->len= len;
		return TERMINATED_STATE;
	}

	if(flag> 0)
	{
		smc= strchr(auth_state.s, ';');
		if(smc== NULL)
		{
			LM_ERR("active or pending state and no expires parameter found");
			return -1;
		}
		if(strncasecmp(smc+1, "expires=", 8))
		{
			LM_ERR("active or pending state and no expires parameter found");
			return -1;
		}
		str_exp.s= smc+ 9;
		str_exp.len= auth_state.s+ auth_state.len- smc- 9;

		if( str2int(&str_exp, (unsigned int*)expires)< 0)
		{
			LM_ERR("while getting int from str\n");
			return -1;
		}
		return flag;
	
	}
	return -1;

error:
	if(res)
	{
		if(res->s)
			pkg_free(res->s);
		pkg_free(res);
	}
	return -1;
}

int rls_handle_notify(struct sip_msg* msg, char* c1, char* c2)
{
	struct to_body *pto, *pfrom= NULL;
	str body= {0, 0};
	ua_pres_t dialog;
	str* res_id= NULL;
	db_key_t query_cols[9], result_cols[1];
	db_val_t query_vals[9];
	db_res_t* result= NULL;
	int n_query_cols= 0;
	str auth_state= {0, 0};
	str* reason= NULL;
	int auth_flag;
	struct hdr_field* hdr= NULL;
	int n, expires= -1;
	str ctype= {0, 0};
	int err_ret = -1;

	LM_DBG("start\n");
	/* extract the dialog information and check if an existing dialog*/	
	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		return -1;
	}
	if((!msg->event ) ||(msg->event->body.len<=0))
	{
		LM_ERR("Missing event header field value\n");
		return -1;
	}
	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return -1;
	}

	pto = get_to(msg);
	if (pto == NULL || pto->error != PARSE_OK) {
		LM_ERR("failed to parse TO header\n");
		return -1;
	}

	memset(&dialog, 0, sizeof(ua_pres_t));
	dialog.watcher_uri= &pto->uri;
    if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{  
		LM_ERR("to tag value not parsed\n");
		goto error;
	}

	dialog.from_tag= pto->tag_value;
	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot parse callid header\n");
		goto error;
	}
	dialog.call_id = msg->callid->body;

	if (!msg->from || !msg->from->body.s)
	{
		LM_ERR("cannot find 'from' header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		LM_DBG("'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			LM_ERR("cannot parse From header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	dialog.pres_uri= &pfrom->uri;
		
	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		goto error;
	}
	dialog.to_tag= pfrom->tag_value;
	dialog.flag|= RLS_SUBSCRIBE;

	dialog.event= get_event_flag(&msg->event->body);
	if(dialog.event< 0)
	{
		LM_ERR("unrecognized event package\n");
		goto error;
	}
	if(pua_get_record_id(&dialog, &res_id)< 0) // verify if within a stored dialog
	{
		LM_ERR("error occured while trying to get dialog record id\n");
		goto error;
	}
	if(res_id== 0)
	{
		LM_DBG("no dialog match found in hash table\n");
		err_ret = 2;
		goto error;
	}

	/* extract the subscription state */
	hdr = get_header_by_static_name( msg, "Subscription-State");
	if( hdr==NULL )
	{
		LM_ERR("'Subscription-State' header not found\n");
		goto error;
	}
	auth_state = hdr->body;

	/* extract state and reason */
	auth_flag= parse_subs_state(auth_state, &reason, &expires);
	if(auth_flag< 0)
	{
		LM_ERR("while parsing 'Subscription-State' header\n");
		goto error;
	}
	if(msg->content_type== NULL || msg->content_type->body.s== NULL)
	{
		LM_DBG("cannot find content type header\n");
	}
	else
		ctype= msg->content_type->body;

	LM_DBG("NOTIFY for [user]= %.*s\n",dialog.pres_uri->len,
			dialog.pres_uri->s);
	/*constructing the xml body*/
	if(get_content_length(msg) == 0 )
	{
		LM_DBG("null body\n");
		goto done;
	}	
	else
	{
		if(ctype.s== 0)
		{
			LM_ERR("content length != 0 and no content type header found\n");
			goto error;
		}
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LM_ERR("cannot extract body from msg\n");
			goto error;
		}
		body.len = get_content_length( msg );
		LM_DBG("[body]= %.*s\n", body.len, body.s);
	}
	
	/* update in rlpres_table where rlsusb_did= res_id and resource_uri= from_uri*/

	query_cols[n_query_cols]= &str_rlsubs_did_col;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *res_id; 
	n_query_cols++;
	
	query_cols[n_query_cols]= &str_resource_uri_col;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *dialog.pres_uri; 
	n_query_cols++;

	query_cols[n_query_cols]= &str_updated_col;
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= UPDATED_TYPE; 
	n_query_cols++;

	query_cols[n_query_cols]= &str_auth_state_col;
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= auth_flag; 
	n_query_cols++;

	if(reason)
	{
		query_cols[n_query_cols]= &str_reason_col;
		query_vals[n_query_cols].type = DB_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val= *reason;
		n_query_cols++;
	}
	query_cols[n_query_cols]= &str_content_type_col;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= ctype;
	n_query_cols++;

	query_cols[n_query_cols]= &str_presence_state_col;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= body;
	n_query_cols++;
	
	query_cols[n_query_cols]= &str_expires_col;
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= expires+ (int)time(NULL);
	n_query_cols++;

	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	/* query-> if not present insert // else update */
	result_cols[0]= &str_updated_col;

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
					2, 1, 0, &result)< 0)
	{
		LM_ERR("in sql query\n");
		if(result)
			rls_dbf.free_result(rls_db, result);
		goto error;
	}
	if(result== NULL)
		goto error;
	n= result->n;
	rls_dbf.free_result(rls_db, result);

	if(n<= 0)
	{
		if(rls_dbf.insert(rls_db, query_cols, query_vals, n_query_cols)< 0)
		{
			LM_ERR("in sql insert\n");
			goto error;
		}
	}
	else
	{
		if(expires!= 0)
		{
			if(rls_dbf.update(rls_db, query_cols, 0, query_vals, query_cols+2,
						query_vals+2, 2, n_query_cols-2)< 0)
			{
				LM_ERR("in sql update\n");
				goto error;
			}
		}
		else  /* if terminated - delete from rls_presentity table */
		{
			if(rls_dbf.delete(rls_db, query_cols, 0, query_vals, 2)< 0)
			{
				LM_ERR("sql delete failed\n");
				goto error;
			}

		}
	}
	
done:
	if( rls_sigb.reply(msg, 200, &su_200_rpl, 0) < 0)
	{
		LM_ERR("failed to send SIP reply\n");
		goto error;
	}	
	pkg_free(res_id->s);
	pkg_free(res_id);

	return 1;

error:
	if(res_id)
	{
		pkg_free(res_id->s);
		pkg_free(res_id);
	}

	return err_ret;
}
/* callid, from_tag, to_tag parameters must be allocated */

int parse_rlsubs_did(char* str_did, str* callid, str* from_tag, str* to_tag)
{
	char* smc= NULL;

	smc= strstr(str_did, DID_SEP);
	if(smc== NULL)
	{
		LM_ERR("bad format for resource list Subscribe dialog"
				" indentifier[rlsubs did]= %s\n", str_did);
		return -1;
	}
	callid->s= str_did;
	callid->len= smc- str_did;
			
	from_tag->s= smc+ DID_SEP_LEN;
	smc= strstr(from_tag->s, DID_SEP);
	if(smc== NULL)
	{
		LM_ERR("bad format for resource list Subscribe dialog"
				" indentifier(rlsubs did)= %s\n", str_did);
		return -1;
	}
	from_tag->len= smc- from_tag->s;
		
	to_tag->s= smc+ DID_SEP_LEN;
	to_tag->len= strlen(str_did)- 2* DID_SEP_LEN- callid->len- from_tag->len;

	return 0;
}

void timer_send_notify(unsigned int ticks,void *param)
{
	db_key_t query_cols[2], update_cols[1], result_cols[7];
	db_val_t query_vals[2], update_vals[1];
	int did_col, resource_uri_col, auth_state_col, reason_col,
		body_col, ctype_col;
	int n_result_cols= 0, i;
	db_res_t *result= NULL;
	char* prev_did= NULL, * curr_did= NULL;
	db_row_t *row;	
	db_val_t *row_vals;
	char* resource_uri;
	str body;
	str callid, to_tag, from_tag;
	xmlDocPtr rlmi_doc= NULL;
	xmlNodePtr list_node= NULL, instance_node= NULL, resource_node;
	unsigned int hash_code= 0;
	int len;
	int size= BUF_REALLOC_SIZE, buf_len= 0;
	char* buf= NULL, *auth_state= NULL;
	int contor= 0, auth_state_flag;
	str bstr= {0, 0};
	str rlmi_cont= {0, 0}, multi_cont;
	subs_t* s, *dialog= NULL;
	char* rl_uri= NULL;
	char* str_aux = NULL;
	str ctype, cid;
	int add_len;

	query_cols[0]= &str_updated_col;
	query_vals[0].type = DB_INT;
	query_vals[0].nul = 0;
	query_vals[0].val.int_val= UPDATED_TYPE; 

	result_cols[did_col= n_result_cols++]= &str_rlsubs_did_col;
	result_cols[resource_uri_col= n_result_cols++]= &str_resource_uri_col;
	result_cols[auth_state_col= n_result_cols++]= &str_auth_state_col;
	result_cols[ctype_col= n_result_cols++]= &str_content_type_col;
	result_cols[reason_col= n_result_cols++]= &str_reason_col;
	result_cols[body_col= n_result_cols++]= &str_presence_state_col;

	/* query in alfabetical order after rlsusbs_did 
	 * (resource list Subscribe dialog indentifier)*/

	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto done;
	}

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
					1, n_result_cols, &str_rlsubs_did_col, &result)< 0)
	{
		LM_ERR("in sql query\n");
		goto done;
	}
	if(result== NULL || result->n<= 0)
		goto done;

	/* update the rlpres table */
	update_cols[0]= &str_updated_col;
	update_vals[0].type = DB_INT;
	update_vals[0].nul = 0;
	update_vals[0].val.int_val= NO_UPDATE_TYPE; 

	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	if(rls_dbf.update(rls_db, query_cols, 0, query_vals, update_cols,
					update_vals, 1, 1)< 0)
	{
		LM_ERR("in sql update\n");
		goto error;
	}

	/* generate the boundary string */

	bstr.s= generate_string((int)time(NULL), BOUNDARY_STRING_LEN);
	if(bstr.s == NULL)
	{
		LM_ERR("failed to generate random string\n");
		goto error;
	}
	bstr.len= strlen(bstr.s);

	/* for the multipart body , use here also an initial allocated
	 * and reallocated on need buffer */
	buf= pkg_malloc(size);
	if(buf== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	LM_DBG("found %d records with updated state\n", result->n);
	for(i= 0; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
		
		curr_did=     (char*)row_vals[did_col].val.string_val;
		resource_uri= (char*)row_vals[resource_uri_col].val.string_val;
		auth_state_flag=     row_vals[auth_state_col].val.int_val;
		body.s=   (char*)row_vals[body_col].val.string_val;
		body.len= strlen(body.s);
		ctype.s = (char*)row_vals[ctype_col].val.string_val;
		ctype.len = strlen(ctype.s);

		/* if all the info for one dialog have been collected -> send notify */
		/* the 'dialog' variable must be filled with the dialog info */
		/* 'buf' must contain the body */
		if(prev_did!= NULL && strcmp(prev_did, curr_did)) 
		{
			xmlDocDumpMemory(rlmi_doc,(xmlChar**)(void*)&rlmi_cont.s,
				&rlmi_cont.len);
		
			multi_cont.s= buf;
			multi_cont.len= buf_len;
			 
			if(agg_body_sendn_update(&dialog->pres_uri, bstr, &rlmi_cont, 
						 (buf_len==0)?NULL:&multi_cont, dialog, hash_code)<0)
			 {
				 LM_ERR("in function agg_body_sendn_update\n");
				 goto error;
			 }
			xmlFree(rlmi_cont.s);

			xmlFreeDoc(rlmi_doc);
			rlmi_doc= NULL;
			pkg_free(rl_uri);
			rl_uri= NULL;
			pkg_free(dialog);
			dialog= NULL;
		}

		/* for the new dialog -> search the dialog info and 
		 * fill the dialog structure and start a new rlmi document */
		if(prev_did== NULL || strcmp(prev_did, curr_did)) 
		{
			/* search the subscription in rlsubs_table*/
			if( parse_rlsubs_did(curr_did, &callid, &from_tag, &to_tag)< 0)
			{
				LM_ERR("bad format for "
					"resource list Subscribe dialog indentifier(rlsubs did)\n");
				prev_did = NULL;
				continue;

			}
			hash_code= core_hash(&callid, &to_tag, hash_size);
		
			lock_get(&rls_table[hash_code].lock);
			s= pres_search_shtable(rls_table,callid,to_tag,from_tag,hash_code);
			if(s== NULL)
			{
				LM_DBG("record not found in hash_table [rlsubs_did]= %s\n",
						curr_did);
				LM_DBG("callid= %.*s\tfrom_tag= %.*s\tto_tag= %.*s\n",
						callid.len, callid.s,from_tag.len,from_tag.s,
						to_tag.len,to_tag.s);
				lock_release(&rls_table[hash_code].lock);
				prev_did = NULL;
				continue;
			}
			LM_DBG("Found rl-subs record in hash table\n");
					
			/* save dialog info and rl_uri*/
			dialog= pres_copy_subs(s, PKG_MEM_TYPE);
			if(dialog== NULL)
			{	
				LM_ERR("while copying subs_t structure\n");
				lock_release(&rls_table[hash_code].lock);
				goto done;
			}
			dialog->expires-= (int)time(NULL);
			lock_release(&rls_table[hash_code].lock);

			/* make new rlmi and multipart documents */
			rlmi_doc= xmlNewDoc(BAD_CAST "1.0");
			if(rlmi_doc== NULL)
			{
				LM_ERR("when creating new xml doc\n");
				goto done;
			}
			list_node= xmlNewNode(NULL, BAD_CAST "list");
			if(list_node== NULL)
			{
				LM_ERR("while creating new xml node\n");
				goto done;
			}
			rl_uri= (char*)pkg_malloc((dialog->pres_uri.len+ 1)* sizeof(char));
			if(rl_uri==  NULL)
			{
				ERR_MEM(PKG_MEM_STR);
			}
			memcpy(rl_uri, dialog->pres_uri.s, dialog->pres_uri.len);
			rl_uri[dialog->pres_uri.len]= '\0';

			xmlNewProp(list_node, BAD_CAST "uri", BAD_CAST rl_uri);
			xmlNewProp(list_node, BAD_CAST "xmlns", BAD_CAST "urn:ietf:params:xml:ns:rlmi");
			xmlNewProp(list_node, BAD_CAST "version", BAD_CAST int2str(dialog->version, &len));
			xmlNewProp(list_node, BAD_CAST "fullState", BAD_CAST "false");

			xmlDocSetRootElement(rlmi_doc, list_node);
			buf_len= 0;

			/* !!!! for now I will include the auth state without checking if 
			 * it has changed - > in future chech if it works */		
		}		

		/* add a node in rlmi_doc and if any presence state registered add 
		 * it in the buffer */
		
		resource_node= xmlNewChild(list_node,NULL,BAD_CAST "resource", NULL);
		if(resource_node== NULL)
		{
			LM_ERR("when adding resource child\n");
			goto done;
		}
		xmlNewProp(resource_node, BAD_CAST "uri", BAD_CAST resource_uri);
			
		/* there might be more records with the same uri- more instances-
		 * search and add them all */
		
		contor= 0;
		while(1)
		{
			contor++;
			cid.s= NULL;
			instance_node= xmlNewChild(resource_node, NULL, 
					BAD_CAST "instance", NULL);
			if(instance_node== NULL)
			{
				LM_ERR("while adding instance child\n");
				goto error;
			}
			str_aux = generate_string(contor, 8);
			if(str_aux == NULL)
			{
				LM_ERR("failed to create random string\n");
				goto error;
			}
			xmlNewProp(instance_node, BAD_CAST "id",
					BAD_CAST str_aux);
			pkg_free(str_aux);

			auth_state= get_auth_string(auth_state_flag);
			if(auth_state== NULL)
			{
				LM_ERR("bad authorization status flag\n");
				goto error;
			}
			xmlNewProp(instance_node, BAD_CAST "state", BAD_CAST auth_state);
		
			if(auth_state_flag & ACTIVE_STATE)
			{
				cid.s= generate_cid(resource_uri, strlen(resource_uri));
				cid.len = strlen(cid.s);
				xmlNewProp(instance_node, BAD_CAST "cid", BAD_CAST cid.s);
			}
			else
			if(auth_state_flag & TERMINATED_STATE)
			{
				xmlNewProp(instance_node, BAD_CAST "reason",
					BAD_CAST row_vals[resource_uri_col].val.string_val);
			}
		
			/* add in the multipart buffer */
			if(cid.s)
			{
				APPEND_MULTIPART_BODY();
				pkg_free(cid.s);
				cid.s = NULL;
			}

			i++;
			if(i== result->n)
			{
				i--;
				break;
			}

			row = &result->rows[i];
			row_vals = ROW_VALUES(row);
	
			if(strncmp(row_vals[resource_uri_col].val.string_val,resource_uri,
					strlen(resource_uri)) || strncmp(curr_did, 
					row_vals[did_col].val.string_val, strlen(curr_did)))
			{
				i--;
				break;
			}
			resource_uri= (char*)row_vals[resource_uri_col].val.string_val;
			auth_state_flag=     row_vals[auth_state_col].val.int_val;
			body.s=   (char*)row_vals[body_col].val.string_val;
			body.len = strlen(body.s);
		}

		prev_did= curr_did;
	}

	if(rlmi_doc)
	{
		xmlDocDumpMemory( rlmi_doc,(xmlChar**)(void*)&rlmi_cont.s,
		&rlmi_cont.len);
		
		multi_cont.s= buf;
		multi_cont.len= buf_len;
	
		 if(agg_body_sendn_update(&dialog->pres_uri, bstr, &rlmi_cont, 
			 (buf_len==0)?NULL:&multi_cont, dialog, hash_code)<0)
		 {
			 LM_ERR("in function agg_body_sendn_update\n");
			 goto error;
		}
		xmlFree(rlmi_cont.s);
		pkg_free(rl_uri);
		rl_uri= NULL;
		pkg_free(dialog);
		dialog= NULL;
	}

error:
done:
	if(result)
		rls_dbf.free_result(rls_db, result);
	if(rlmi_doc)
		xmlFreeDoc(rlmi_doc);
	if(rl_uri)
		pkg_free(rl_uri);
	if(bstr.s)
		pkg_free(bstr.s);
		
	if(buf)
		pkg_free(buf);
	if(dialog)
		pkg_free(dialog);
	return;
}


/* function to periodicaly clean the rls_presentity table */

void rls_presentity_clean(unsigned int ticks,void *param)
{
	db_key_t query_cols[2];
	db_op_t query_ops[2];
	db_val_t query_vals[2];

	query_cols[0]= &str_expires_col;
	query_ops[0]= OP_LT;
	query_vals[0].nul= 0;
	query_vals[0].type= DB_INT;
	query_vals[0].val.int_val= (int)time(NULL)-10;

	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return ;
	}

	if(rls_dbf.delete(rls_db, query_cols, query_ops, query_vals, 1)< 0)
	{
		LM_ERR("in sql delete\n");
		return ;
	}

}
