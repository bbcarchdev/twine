/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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

/* A single entry in a list of multi-lingual literals */
struct literal_struct
{
	const char *lang;
	librdf_node *node;
	int priority;
};

/* The matching state for a single property; 'map' points to the predicate
 * mapping data (defined above).
 *
 * If the mapping specifies that a non-datatyped literal is expected then the
 * current state is maintained in the 'literals' list, which will contain
 * one entry per language, including if applicable an entry where lang==NULL.
 *
 * Otherwise, 'resource' will be a clone of the the object of the relevant
 * candidate triple with the highest priority, and 'priority' will be the
 * corresponding priority value from the predicate-matching structure.
 *
 */
struct propmatch_struct
{
	struct spindle_predicatemap_struct *map;
	int priority;
	int prominence;
	librdf_node *resource;
	struct literal_struct *literals;
	size_t nliterals;
};

/* Current property matching state data */
struct propdata_struct
{
	SPINDLE *spindle;
	SPINDLECACHE *cache;
	const char *localname;
	const char *classname;
	librdf_node *context;
	librdf_model *source;
	librdf_model *proxymodel;
	librdf_model *rootmodel;
	struct spindle_predicatemap_struct *maps;
	struct propmatch_struct *matches;
};

static int spindle_prop_init_(struct propdata_struct *data, SPINDLECACHE *cache);
static int spindle_prop_cleanup_(struct propdata_struct *data);
static int spindle_prop_modified_(struct propdata_struct *data);
static int spindle_prop_loop_(struct propdata_struct *data);
static int spindle_prop_test_(struct propdata_struct *data, librdf_statement *st, const char *predicate);
static int spindle_prop_candidate_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_uri_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_literal_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_lang_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj, const char *lang);

static int spindle_prop_apply_(struct propdata_struct *data);

int
spindle_prop_update(SPINDLECACHE *cache)
{
	struct propdata_struct data;
	int r;

	if(spindle_prop_init_(&data, cache))
	{
		return -1;
	}

	r = spindle_prop_loop_(&data);
	if(!r)
	{
		r = spindle_prop_apply_(&data);
	}

	spindle_prop_modified_(&data);

	spindle_prop_cleanup_(&data);

	return r;
}

/* Initialise the property data structure */
static int
spindle_prop_init_(struct propdata_struct *data, SPINDLECACHE *cache)
{
	size_t c;

	memset(data, 0, sizeof(struct propdata_struct));
	data->spindle = cache->spindle;
	data->cache = cache;
	data->source = cache->sourcedata;
	data->localname = cache->localname;
	data->classname = cache->classname;
	data->proxymodel = cache->proxydata;
	data->rootmodel = cache->rootdata;
	data->context = cache->graph;
	data->maps = cache->spindle->predicates;
	data->matches = (struct propmatch_struct *) calloc(cache->spindle->predcount + 1, sizeof(struct propmatch_struct));
	if(!data->matches)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for property match state\n");
		return -1;
	}
	for(c = 0; c < cache->spindle->predcount; c++)
	{
		data->matches[c].map = &(data->maps[c]);
	}
	return 0;
}

/* Clean up the resources used by a property data structure */
static int
spindle_prop_cleanup_(struct propdata_struct *data)
{
	size_t c, d;

	if(data->matches)
	{
		for(c = 0; data->matches[c].map && data->matches[c].map->target; c++)
		{
			if(data->matches[c].resource)
			{
				librdf_free_node(data->matches[c].resource);
			}
			for(d = 0; d < data->matches[c].nliterals; d++)
			{
				if(data->matches[c].literals[d].node)
				{
					librdf_free_node(data->matches[c].literals[d].node);
				}
			}
			free(data->matches[c].literals);
		}
		free(data->matches);
	}
	return 0;
}

/* Add a dct:modified triple to the proxy data */
static int 
spindle_prop_modified_(struct propdata_struct *data)
{
	librdf_node *obj;
	librdf_statement *st;
	char tbuf[64];
	time_t t;
	struct tm now;

	t = time(NULL);
	gmtime_r(&t, &now);
	strftime(tbuf, sizeof(tbuf) - 1, "%Y-%m-%dT%H:%M:%SZ", &now);
	st = twine_rdf_st_create();
	if(!st) return -1;
	obj = twine_rdf_node_clone(data->cache->self);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_subject(st, obj);
	obj = twine_rdf_node_clone(data->spindle->modified);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_predicate(st, obj);
	obj = librdf_new_node_from_typed_literal(data->spindle->world, (const unsigned char *) tbuf, NULL, data->spindle->xsd_dateTime);
	if(!obj)
	{
		twine_logf(LOG_CRIT, "failed to create new node for \"%s\"^^xsd:dateTime\n");
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_object(st, obj);
	twine_rdf_model_add_st(data->proxymodel, st, data->context);
	if(data->spindle->multigraph)
	{
		/* Also add the statement to the root graph */
		twine_rdf_model_add_st(data->rootmodel, st, data->spindle->rootgraph);
	}
	twine_rdf_st_destroy(st);
	return 0;
}

/* Loop over a model and process any known predicates */
static int
spindle_prop_loop_(struct propdata_struct *data)
{
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *pred;
	librdf_uri *puri;
	const char *pstr;
	int r;

	query = librdf_new_statement(data->spindle->world);
	stream = librdf_model_find_statements(data->source, query);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);	
		pred = librdf_statement_get_predicate(st);
		if(librdf_node_is_resource(pred) &&
		   (puri = librdf_node_get_uri(pred)) &&
		   (pstr = (const char *) librdf_uri_as_string(puri)))
		{
			r = spindle_prop_test_(data, st, pstr);
			if(r < 0)
			{
				break;
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return (r < 0 ? -1 : 0);
}

/* Apply scored matches to the proxy model ready for insertion */
static int
spindle_prop_apply_(struct propdata_struct *data)
{
	size_t c, d;
	librdf_node *node;
	librdf_statement *base, *pst, *lpst;
	int r;

	/* Generate a model containing the new data for the proxy */
	node = twine_rdf_node_clone(data->cache->self);
	if(!node) return -1;

	base = twine_rdf_st_create();
	if(!base)
	{
		librdf_free_node(node);
		return -1;
	}
	librdf_statement_set_subject(base, node);

	r = 0;
	for(c = 0; !r && data->matches[c].map && data->matches[c].map->target; c++)
	{
		data->cache->score -= data->matches[c].prominence;
		pst = twine_rdf_st_clone(base);
		if(!pst)
		{
			r = -1;
			break;		
		}
		node = twine_rdf_node_createuri(data->matches[c].map->target);
		if(!node)
		{
			librdf_free_statement(pst);
			r = -1;
			break;
		}
		librdf_statement_set_predicate(pst, node);		
		if(data->matches[c].resource)
		{
			twine_logf(LOG_DEBUG, "==> Property <%s>\n", data->matches[c].map->target);		
			librdf_statement_set_object(pst, data->matches[c].resource);
			data->matches[c].resource = NULL;
			if(twine_rdf_model_add_st(data->proxymodel, pst, data->context))
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to model\n");
				r = -1;
			}
			if(!r && data->matches[c].map->indexed && data->spindle->multigraph)
			{
				if(twine_rdf_model_add_st(data->rootmodel, pst, data->spindle->rootgraph))
				{
					twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to model\n");
					r = -1;
				}
			}
		}
		else
		{
			if(!r && data->matches[c].nliterals)
			{
				twine_logf(LOG_DEBUG, "==> Property <%s>\n", data->matches[c].map->target);
			}
			for(d = 0; !r && d < data->matches[c].nliterals; d++)
			{
				lpst = twine_rdf_st_clone(pst);
				if(!lpst)
				{
					r = -1;
					break;
				}
				librdf_statement_set_object(lpst, data->matches[c].literals[d].node);
				data->matches[c].literals[d].node = NULL;
				if(twine_rdf_model_add_st(data->proxymodel, lpst, data->context))
				{
					twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to model\n");
					r = -1;
				}
				if(!r && data->matches[c].map->indexed && data->spindle->multigraph)
				{
					if(twine_rdf_model_add_st(data->rootmodel, lpst, data->spindle->rootgraph))
					{
						twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to model\n");
						r = -1;
					}
				}
				librdf_free_statement(lpst);
			}
			librdf_free_statement(pst);
		}
	}
	librdf_free_statement(base);
	return r;
}

/* Determine whether a given statement should be processed, and do so if so */
static int
spindle_prop_test_(struct propdata_struct *data, librdf_statement *st, const char *predicate)
{
	size_t c, d;
	librdf_node *obj;

	for(c = 0; data->maps[c].target; c++)
	{
		if(!data->maps[c].matches)
		{
			continue;
		}
		for(d = 0; data->maps[c].matches[d].predicate; d++)
		{
			if(data->maps[c].matches[d].onlyfor &&
			   (!data->classname || strcmp(data->maps[c].matches[d].onlyfor, data->classname)))
			{
				continue;
			}
			if(!strcmp(predicate, data->maps[c].matches[d].predicate))
			{
				obj = librdf_statement_get_object(st);
				spindle_prop_candidate_(data, &(data->matches[c]), &(data->maps[c].matches[d]), st, obj);
				break;
			}
		}
	}
	return 0;
}

/* The statement is a candidate for caching by the proxy; if it's not already
 * beaten by a high-priority alternative, store it
 */
static int
spindle_prop_candidate_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj)
{
	switch(match->map->expected)
	{
	case RAPTOR_TERM_TYPE_UNKNOWN:
		/* No-op */
		break;
	case RAPTOR_TERM_TYPE_URI:
		if(!librdf_node_is_resource(obj))
		{
			break;
		}
		return spindle_prop_candidate_uri_(data, match, criteria, st, obj);
	case RAPTOR_TERM_TYPE_LITERAL:
		if(!librdf_node_is_literal(obj))
		{
			break;
		}
		return spindle_prop_candidate_literal_(data, match, criteria, st, obj);
	case RAPTOR_TERM_TYPE_BLANK:
		break;
		/* Not implemented */
	}
	return 0;
}

static int
spindle_prop_candidate_uri_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj)
{
	librdf_node *node, *newobj;
	librdf_statement *newst;
	char *uri;

	(void) st;

	if(criteria->priority && match->priority <= criteria->priority)
	{
		/* We already have a better match for the property */
		return 0;
	}	
	/* Some resource properties, such as depiction, are used as-is;
	 * others are only used if there's a proxy corresponding to the
	 * object URI.
	 */	
	newobj = NULL;
	if(match->map->proxyonly)
	{
		uri = spindle_proxy_locate(data->spindle, (const char *) librdf_uri_as_string(librdf_node_get_uri(obj)));
		if(!uri || !strcmp(uri, data->localname))
		{
			free(uri);
			return 0;
		}
		newobj = twine_rdf_node_createuri(uri);
		if(!newobj)
		{
			free(uri);
			return -1;
		}
		free(uri);
	}
   
	/* If the priority is zero, the triple is added to the proxy model
	 * immediately.
	 */
	if(!criteria->priority)
	{
		newst = twine_rdf_st_create();
		if(!newst)		   
		{
			twine_rdf_node_destroy(newobj);
			return -1;
		}
		node = twine_rdf_node_clone(data->cache->self);
		if(!node)
		{
			twine_rdf_node_destroy(newobj);
			twine_rdf_st_destroy(newst);
			return -1;
		}
		librdf_statement_set_subject(newst, node);
		node = twine_rdf_node_createuri(match->map->target);
		if(!node)
		{
			twine_rdf_node_destroy(newobj);
			twine_rdf_st_destroy(newst);
			return -1;
		}
		twine_logf(LOG_DEBUG, "==> Property <%s>\n", match->map->target);
		librdf_statement_set_predicate(newst, node);
		if(newobj)
		{
			librdf_statement_set_object(newst, newobj);
			newobj = NULL;
		}
		else
		{
			node = twine_rdf_node_clone(obj);
			if(!node)
			{
				twine_rdf_st_destroy(newst);
				return -1;
			}
			librdf_statement_set_object(newst, node);
		}
		twine_rdf_model_add_st(data->proxymodel, newst, data->context);
		twine_rdf_st_destroy(newst);
		if(criteria->prominence)
		{
			data->cache->score -= criteria->prominence;
		}
		else
		{
			data->cache->score -= match->map->prominence;
		}
		return 1;
	}
	if(newobj)
	{
		node = newobj;
		newobj = NULL;
	}
	else
	{
		node = twine_rdf_node_clone(obj);
		if(!node)
		{
			return -1;
		}
	}
	twine_rdf_node_destroy(match->resource);
	match->resource = node;
	match->priority = criteria->priority;
	if(criteria->prominence)
	{
		match->prominence = criteria->prominence;
	}
	else
	{
		match->prominence = match->map->prominence;
	}
	return 1;
}

static int
spindle_dt_is_int(const char *dtstr)
{
	if(!strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#integer") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#long") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#short") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#byte") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#int") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#nonPositiveInteger") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#nonNegativeInteger") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#negativeInteger") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#positiveInteger") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedLong") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedInt") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedShort") ||
	   !strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedByte"))
	{
		return 1;
	}
	return 0;
}

static int
spindle_prop_candidate_literal_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj)
{
	char *lang;
	librdf_uri *dturi, *uri;
	librdf_node *node;
	const char *dtstr;

	lang = librdf_node_get_literal_value_language(obj);
	if(!match->map->datatype)
	{
		/* If there's no datatype specified, match per language */
		return spindle_prop_candidate_lang_(data, match, criteria, st, obj, lang);
	}
	if(match->priority && match->priority <= criteria->priority)
	{
		return 0;
	}
	/* The datatype must either match, or be unset (and if unset, the
	 * literal must not have a language).
	 */
	if((dturi = librdf_node_get_literal_value_datatype_uri(obj)))
	{
		dtstr = (const char *) librdf_uri_as_string(dturi);
	}	
	else if(lang)
	{
		return 0;
	}
	else
	{
		dtstr = NULL;
	}
	if(dtstr)
	{
		/* Coerce specific types */
		if(!strcmp(match->map->datatype, "http://www.w3.org/2001/XMLSchema#decimal"))
		{
			if(spindle_dt_is_int(dtstr))
			{
				dtstr = match->map->datatype;
			}
		}
	}
	if(!dtstr || !strcmp(dtstr, match->map->datatype))
	{
		uri = librdf_new_uri(data->spindle->world, (const unsigned char *) match->map->datatype);
		if(!uri)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create new URI for <%s>\n", match->map->datatype);
		}
		node = librdf_new_node_from_typed_literal(data->spindle->world,
												  librdf_node_get_literal_value(obj),
												  NULL,
												  uri);
		librdf_free_uri(uri);
		if(!node)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create new node for typed literal\n");
			return -1;
		}
		twine_rdf_node_destroy(match->resource);
		match->resource = node;
		match->priority = criteria->priority;
		if(criteria->prominence)
		{
			match->prominence = criteria->prominence;
		}
		else
		{
			match->prominence = match->map->prominence;
		}
		return 1;
	}
	return 0;
}

static int
spindle_prop_candidate_lang_(struct propdata_struct *data, struct propmatch_struct *match, struct spindle_predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj, const char *lang)
{
	struct literal_struct *entry, *p;
	size_t c;
	librdf_node *node;

	(void) data;
	(void) st;

	entry = NULL;
	for(c = 0; c < match->nliterals; c++)
	{
		if((!lang && !match->literals[c].lang) ||
		   (lang && match->literals[c].lang &&
			!strcasecmp(match->literals[c].lang, lang)))
		{
			entry = &(match->literals[c]);
			break;
		}
	}
	if(entry && entry->priority <= criteria->priority)
	{
		return 0;
	}
	if(!entry)
	{
		p = (struct literal_struct *) realloc(match->literals, sizeof(struct literal_struct) * (match->nliterals + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to reallocate literals match structure\n");
			return -1;
		}
		match->literals = p;
		entry = &(match->literals[match->nliterals]);
		memset(entry, 0, sizeof(struct literal_struct));
		entry->lang = lang;
		match->nliterals++;
	}
	node = twine_rdf_node_clone(obj);
	if(!node)
	{
		return -1;
	}
	twine_rdf_node_destroy(entry->node);
	entry->node = node;
	entry->priority = criteria->priority;
	if(criteria->prominence)
	{
		match->prominence = criteria->prominence;
	}
	else
	{
		match->prominence = match->map->prominence;
	}
	return 1;
}
