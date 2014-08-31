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

struct predicatematch_struct
{
	int priority;
	const char *predicate;
	const char *onlyfor;
};

struct predicatemap_struct
{
	const char *target;
	struct predicatematch_struct *matches;
	raptor_term_type expected;
	const char *datatype;
};

struct literal_struct
{
	const char *lang;
	librdf_node *node;
	int priority;
};

struct propmatch_struct
{
	struct predicatemap_struct *map;
	int priority;
	librdf_node *resource;
	struct literal_struct *literals;
	size_t nliterals;
};

struct propdata_struct
{
	const char *localname;
	const char *classname;
	librdf_node *context;
	librdf_model *source;
	librdf_model *proxymodel;
	struct predicatemap_struct *maps;
	struct propmatch_struct *matches;
};

static struct predicatematch_struct label_match[] = {
	{ 20, "http://www.w3.org/2008/05/skos#prefLabel", NULL },
	{ 21, "http://www.w3.org/2004/02/skos/core#prefLabel", NULL },
	{ 30, "http://xmlns.com/foaf/0.1/name", "http://xmlns.com/foaf/0.1/Person" },
	{ 30, "http://xmlns.com/foaf/0.1/name", "http://xmlns.com/foaf/0.1/Group" },
	{ 30, "http://xmlns.com/foaf/0.1/name", "http://xmlns.com/foaf/0.1/Agent" },
	{ 30, "http://www.geonames.org/ontology#name", "http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing" },
	{ 35, "http://www.geonames.org/ontology#alternateName", "http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing" },
	{ 40, "http://purl.org/dc/terms/title", NULL },
	{ 41, "http://purl.org/dc/elements/1.1/title", NULL },
	{ 50, "http://www.w3.org/2000/01/rdf-schema#label", NULL },	
	{ -1, NULL, NULL }
};

static struct predicatematch_struct description_match[] = {
	{ 40, "http://purl.org/dc/terms/description", NULL },
	{ 41, "http://purl.org/dc/elements/1.1/description", NULL },
	{ 50, "http://www.w3.org/2000/01/rdf-schema#comment", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct lat_match[] = {
	{ -1, NULL, NULL }
};

static struct predicatematch_struct long_match[] = {
	{ -1, NULL, NULL }
};

static struct predicatematch_struct depiction_match[] = {
	{ 0, "http://xmlns.com/foaf/0.1/depiction", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct subject_match[] = {
	{ 0, "http://purl.org/dc/terms/subject", NULL },
	{ 0, "http://purl.org/dc/elements/1.1/subject", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct inscheme_match[] = {
	{ -1, NULL, NULL }
};

static struct predicatematch_struct broader_match[] = {
	{ -1, NULL, NULL }
};

static struct predicatematch_struct narrower_match[] = {
	{ -1, NULL, NULL }
};

static struct predicatematch_struct part_match[] = {
	{ 0, "http://purl.org/dc/terms/isPartOf", NULL },
	{ -1, NULL, NULL }
};

static struct predicatemap_struct predicatemap[] = {
	{
		"http://www.w3.org/2000/01/rdf-schema#label",
		label_match,
		RAPTOR_TERM_TYPE_LITERAL,
		NULL,
	},
	{
		"http://purl.org/dc/terms/description",
		description_match,
		RAPTOR_TERM_TYPE_LITERAL,
		NULL,
	},
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#lat",
		lat_match,
		RAPTOR_TERM_TYPE_LITERAL,
		"http://www.w3.org/2001/XMLSchema#decimal",
	},
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#long",
		long_match,
		RAPTOR_TERM_TYPE_LITERAL,
		"http://www.w3.org/2001/XMLSchema#decimal"
	},
	{
		"http://xmlns.com/foaf/0.1/depiction",
		depiction_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://www.w3.org/2004/02/skos/core#inScheme",
		inscheme_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://www.w3.org/2004/02/skos/core#broader",
		broader_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://www.w3.org/2004/02/skos/core#narrower",
		narrower_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://purl.org/dc/terms/subject",
		subject_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://purl.org/dc/terms/isPartOf",
		part_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},

	{ NULL, NULL, RAPTOR_TERM_TYPE_UNKNOWN, NULL }
};

static int spindle_prop_init_(struct propdata_struct *data, const char *localname, librdf_model *model, const char *classname, librdf_model *proxymodel, librdf_node *graph);
static int spindle_prop_cleanup_(struct propdata_struct *data);

static int spindle_prop_loop_(struct propdata_struct *data);
static int spindle_prop_test_(struct propdata_struct *data, librdf_statement *st, const char *predicate);
static int spindle_prop_candidate_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_uri_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_literal_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_lang_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj, const char *lang);

static int spindle_prop_apply_(struct propdata_struct *data);

int
spindle_prop_update(const char *localname, librdf_model *model, const char *classname, librdf_model *proxymodel, librdf_node *graph)
{
	struct propdata_struct data;
	int r;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": updating properties for <%s>\n", localname);
	if(spindle_prop_init_(&data, localname, model, classname, proxymodel, graph))
	{
		return -1;
	}

	r = spindle_prop_loop_(&data);
	if(!r)
	{
		r = spindle_prop_apply_(&data);
	}

	spindle_prop_cleanup_(&data);

	return r;
}

/* Initialise the property data structure */
static int
spindle_prop_init_(struct propdata_struct *data, const char *localname, librdf_model *model, const char *classname, librdf_model *proxymodel, librdf_node *graph)
{
	size_t c;

	memset(data, 0, sizeof(struct propdata_struct));
	data->source = model;
	data->localname = localname;
	data->classname = classname;
	data->proxymodel = proxymodel;
	data->context = graph;
	data->maps = predicatemap;
	data->matches = (struct propmatch_struct *) calloc(sizeof(predicatemap) / sizeof(struct predicatemap_struct), sizeof(struct propmatch_struct));
	if(!data->matches)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for prop matches\n");
		return -1;
	}
	for(c = 0; predicatemap[c].target; c++)
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

	query = librdf_new_statement(spindle_world);
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
	node = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) data->localname);
	if(!node)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for <%s>\n", data->localname);
		return -1;
	}

	base = librdf_new_statement(spindle_world);	
	if(!base)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME, ": failed to create statement\n");
		librdf_free_node(node);
		return -1;
	}
	librdf_statement_set_subject(base, node);

	r = 0;
	for(c = 0; !r && data->matches[c].map && data->matches[c].map->target; c++)
	{
		pst = librdf_new_statement_from_statement(base);		
		if(!pst)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to clone statement\n");
			r = -1;
			break;		
		}
		node = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) data->matches[c].map->target);
		if(!node)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create new node for <%s>\n", data->matches[c].map->target);
			librdf_free_statement(pst);
			r = -1;
			break;
		}
		librdf_statement_set_predicate(pst, node);		

		if(data->matches[c].resource)
		{
			librdf_statement_set_object(pst, data->matches[c].resource);
			data->matches[c].resource = NULL;
			if(librdf_model_context_add_statement(data->proxymodel, data->context, pst))
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to model\n");
				r = -1;
			}
		}
		else
		{
			for(d = 0; !r && d < data->matches[c].nliterals; d++)
			{
				lpst = librdf_new_statement_from_statement(pst);
				if(!lpst)
				{
					twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to clone statement\n");
					r = -1;
					break;
				}
				/* TODO: datatype override */
				librdf_statement_set_object(lpst, data->matches[c].literals[d].node);
				data->matches[c].literals[d].node = NULL;
				if(librdf_model_context_add_statement(data->proxymodel, data->context, lpst))
				{
					twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to model\n");
					r = -1;
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
		for(d = 0; data->maps[c].matches[d].predicate; d++)
		{
			if(data->maps[c].matches[d].onlyfor &&
			   strcmp(data->maps[c].matches[d].onlyfor, data->classname))
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
spindle_prop_candidate_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj)
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
spindle_prop_candidate_uri_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj)
{
	librdf_node *node;

	(void) st;
	(void) data;

	if(!criteria->priority)
	{
		/* TODO: Add the resource to the proxy model immediately */
		return 0;
	}
	if(match->priority <= criteria->priority)
	{
		return 0;
	}
	node = librdf_new_node_from_node(obj);
	if(!node)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate node\n");
		return -1;
	}
	if(match->resource)
	{
		librdf_free_node(match->resource);
	}
	match->resource = node;
	match->priority = criteria->priority;
	return 1;
}

static int
spindle_prop_candidate_literal_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj)
{
	char *lang;
	librdf_uri *dturi;
	const char *dtstr;

	lang = librdf_node_get_literal_value_language(obj);
	if(!match->map->datatype)
	{
		/* If there's no datatype specified, match per language */
		return spindle_prop_candidate_lang_(data, match, criteria, st, obj, lang);
	}
	if(match->priority <= criteria->priority)
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
	if(!dtstr || !strcmp(dtstr, match->map->datatype))
	{
		if(match->resource)
		{
			librdf_free_node(match->resource);
		}
		match->resource = librdf_new_node_from_node(obj);
		match->priority = criteria->priority;
		return 1;
	}
	return 0;
}

static int
spindle_prop_candidate_lang_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj, const char *lang)
{
	struct literal_struct *entry, *p;
	size_t c;

	(void) data;
	(void) st;

	entry = NULL;
	for(c = 0; c < match->nliterals; c++)
	{
		if((!lang && !match->literals[c].lang) ||
		   (lang && match->literals[c].lang &&
			!strcmp(match->literals[c].lang, lang)))
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
	if(entry->node)
	{
		librdf_free_node(entry->node);
	}
	entry->node = librdf_new_node_from_node(obj);
	entry->priority = criteria->priority;
	return 1;
}
