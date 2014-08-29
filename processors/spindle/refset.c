/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_spindle.h"

/* Extract a list of co-references from a librdf model */
struct spindle_corefset_struct *
spindle_coref_extract(librdf_model *model, const char *graphuri)
{
	struct spindle_corefset_struct *set;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *subj, *obj, *pred;
	librdf_uri *uri;
	unsigned char *l, *r;
	
	(void) graphuri;

	set = (struct spindle_corefset_struct *) calloc(1, sizeof(struct spindle_corefset_struct));
	if(!set)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for new coreference set\n");
		return NULL;
	}
	query = librdf_new_statement(spindle_world);
	pred = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) "http://www.w3.org/2002/07/owl#sameAs");
	librdf_statement_set_predicate(query, pred);
	/* pred is now owned by query */
	pred = NULL;
	stream = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		subj = librdf_statement_get_subject(st);
		obj = librdf_statement_get_object(st);
		if(librdf_node_is_resource(subj) && librdf_node_is_resource(obj))
		{
			uri = librdf_node_get_uri(subj);
			l = librdf_uri_as_string(uri);
			uri = librdf_node_get_uri(obj);
			r = librdf_uri_as_string(uri);
			if(spindle_coref_add(set, (const char *) l, (const char *) r))
			{
				spindle_coref_destroy(set);
				set = NULL;
				break;
			}
		}			
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return set;
}

/* Add a single co-reference to a set */
int
spindle_coref_add(struct spindle_corefset_struct *set, const char *l, const char *r)
{
	struct spindle_coref_struct *p;
	
	if(set->refcount >= set->size)
	{
		p = (struct spindle_coref_struct *) realloc(set->refs, sizeof(struct spindle_coref_struct) * (set->size + SET_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand size of coreference set\n");
			return -1;
		}
		set->refs = p;
		set->size += SET_BLOCKSIZE;
	}
	p = &(set->refs[set->refcount]);
	memset(p, 0, sizeof(struct spindle_coref_struct));
	p->left = strdup(l);
	p->right = strdup(r);
	if(!p->left || !p->right)
	{
		free(p->left);
		free(p->right);
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate URIs when adding coreference to set\n");
		return -1;
	}
	set->refcount++;
	return 0;
}

/* Free the resources used by a co-reference set */
int
spindle_coref_destroy(struct spindle_corefset_struct *set)
{
	size_t c;

	for(c = 0; c < set->refcount; c++)
	{
		free(set->refs[c].left);
		free(set->refs[c].right);
	}
	free(set->refs);
	free(set);
	return 0;
}

