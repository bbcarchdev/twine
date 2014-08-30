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

typedef int (*predicatefilter_fn)(librdf_node *in, librdf_node **out);

struct predicatematch_struct
{
	int priority;
	const char *predicate;
	const char *onlyfor;
};

struct predicatemap_struct
{
	const char *target;
	predicatefilter_fn filter;
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

struct propertymatch_struct
{
	struct predicatemap_struct *map;
	int priority;
	librdf_node *resource;
	struct literal_struct *literals;
	size_t nliterals;
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


static struct predicatemap_struct predicatemap[] = {
	{
		"http://www.w3.org/2000/01/rdf-schema#label",
		NULL,
		label_match,
		RAPTOR_TERM_TYPE_LITERAL,
		NULL,
	},
	{
		"http://purl.org/dc/terms/description",
		NULL,
		description_match,
		RAPTOR_TERM_TYPE_LITERAL,
		NULL,
	},
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#lat",
		NULL,
		lat_match,
		RAPTOR_TERM_TYPE_LITERAL,
		"http://www.w3.org/2001/XMLSchema#decimal",
	},
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#long",
		NULL,
		long_match,
		RAPTOR_TERM_TYPE_LITERAL,
		"http://www.w3.org/2001/XMLSchema#decimal"
	},
	{
		"http://xmlns.com/foaf/0.1/depiction",
		NULL,
		depiction_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://www.w3.org/2004/02/skos/core#inScheme",
		NULL,
		inscheme_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://www.w3.org/2004/02/skos/core#broader",
		NULL,
		broader_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://www.w3.org/2004/02/skos/core#narrower",
		NULL,
		narrower_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},
	{
		"http://purl.org/dc/terms/subject",
		NULL, 
		subject_match,
		RAPTOR_TERM_TYPE_URI,
		NULL
	},

	{ NULL, NULL, NULL, RAPTOR_TERM_TYPE_UNKNOWN, NULL }
};

static int spindle_predicate_test_(struct propertymatch_struct *matches, librdf_statement *st, const char *predicate, const char *classname);

static int spindle_predicate_candidate_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname);

static int spindle_predicate_candidate_uri_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname, librdf_node *obj);

static int spindle_predicate_candidate_literal_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname, librdf_node *obj);

static int spindle_predicate_candidate_lang_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname, librdf_node *obj, const char *lang);

static int spindle_predicate_apply_(librdf_model *model, const char *localname, struct propertymatch_struct *matches);

static int spindle_predicate_destroy_(struct propertymatch_struct *matches);

int
spindle_predicate_update(const char *localname, librdf_model *model, const char *classname)
{
	struct propertymatch_struct *matches;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *pred;
	librdf_uri *puri;
	const char *pstr;
	size_t c;
	librdf_model *proxymodel;
	int r;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": updating properties for <%s>\n", localname);
	matches = (struct propertymatch_struct *) calloc(sizeof(predicatemap) / sizeof(struct predicatemap_struct), sizeof(struct propertymatch_struct));
	if(!matches)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for property matches\n");
		return -1;
	}
	for(c = 0; predicatemap[c].target; c++)
	{
		matches[c].map = &(predicatemap[c]);
	}
	query = librdf_new_statement(spindle_world);
	stream = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);	
		pred = librdf_statement_get_predicate(st);
		if(librdf_node_is_resource(pred) &&
		   (puri = librdf_node_get_uri(pred)) &&
		   (pstr = (const char *) librdf_uri_as_string(puri)))
		{
			spindle_predicate_test_(matches, st, pstr, classname);
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);

	proxymodel = twine_rdf_model_create();
	r = spindle_predicate_apply_(proxymodel, localname, matches);
	if(!r)
	{
		r = sparql_insert_model(spindle_sparql, proxymodel);
	}
	spindle_predicate_destroy_(matches);	
	librdf_free_model(proxymodel);
	return r;
}

static int
spindle_predicate_destroy_(struct propertymatch_struct *matches)
{
	size_t c, d;

	for(c = 0; matches[c].map && matches[c].map->target; c++)
	{
		if(matches[c].resource)
		{
			librdf_free_node(matches[c].resource);
		}
		for(d = 0; d < matches[c].nliterals; d++)
		{
			if(matches[c].literals[d].node)
			{
				librdf_free_node(matches[c].literals[d].node);
			}
		}
		free(matches[c].literals);
	}
	free(matches);
	return 0;
}
		

static int
spindle_predicate_apply_(librdf_model *proxymodel, const char *localname, struct propertymatch_struct *matches)
{
	size_t c, d;
	librdf_node *context, *node;
	librdf_statement *base, *pst, *lpst;
	int r;

	/* Generate a model containing the new data for the proxy */
	context = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) spindle_root);
	if(!context)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for <%s>\n", spindle_root);
		return -1;
	}
	node = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) localname);
	if(!node)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for <%s>\n", localname);
		librdf_free_node(context);
		return -1;
	}

	base = librdf_new_statement(spindle_world);	
	if(!base)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME, ": failed to create statement\n");
		librdf_free_node(context);
		librdf_free_node(node);
		return -1;
	}
	librdf_statement_set_subject(base, node);

	r = 0;
	for(c = 0; !r && matches[c].map && matches[c].map->target; c++)
	{
		pst = librdf_new_statement_from_statement(base);		
		if(!pst)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to clone statement\n");
			r = -1;
			break;		
		}
		node = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) matches[c].map->target);
		if(!node)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create new node for <%s>\n", matches[c].map->target);
			librdf_free_statement(pst);
			r = -1;
			break;
		}
		librdf_statement_set_predicate(pst, node);		

		if(matches[c].resource)
		{
			librdf_statement_set_object(pst, matches[c].resource);
			matches[c].resource = NULL;
			if(librdf_model_context_add_statement(proxymodel, context, pst))
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to model\n");
				r = -1;
			}
		}
		else
		{
			for(d = 0; !r && d < matches[c].nliterals; d++)
			{
				lpst = librdf_new_statement_from_statement(pst);
				if(!lpst)
				{
					twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to clone statement\n");
					r = -1;
					break;
				}
				/* TODO: datatype override */
				librdf_statement_set_object(lpst, matches[c].literals[d].node);
				matches[c].literals[d].node = NULL;
				if(librdf_model_context_add_statement(proxymodel, context, lpst))
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
	librdf_free_node(context);
	return r;
}	

static int
spindle_predicate_test_(struct propertymatch_struct *matches, librdf_statement *st, const char *predicate, const char *classname)
{
	size_t c, d;

	for(c = 0; predicatemap[c].target; c++)
	{
		for(d = 0; predicatemap[c].matches[d].predicate; d++)
		{
			if(predicatemap[c].matches[d].onlyfor &&
			   strcmp(predicatemap[c].matches[d].onlyfor, classname))
			{
				continue;
			}
			if(!strcmp(predicate, predicatemap[c].matches[d].predicate))
			{
				spindle_predicate_candidate_(&(matches[c]), &(predicatemap[c].matches[d]), st, classname);
				break;
			}
		}
	}
	return 0;
}

static int
spindle_predicate_candidate_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname)
{
	librdf_node *obj;

	if(criteria->onlyfor && strcmp(classname, criteria->onlyfor))
	{
		return 0;
	}
	obj = librdf_statement_get_object(st);
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
		return spindle_predicate_candidate_uri_(match, criteria, st, classname, obj);
	case RAPTOR_TERM_TYPE_LITERAL:
		if(!librdf_node_is_literal(obj))
		{
			break;
		}
		return spindle_predicate_candidate_literal_(match, criteria, st, classname, obj);	
	case RAPTOR_TERM_TYPE_BLANK:
		break;
		/* Not implemented */
	}
	return 0;
}

static int
spindle_predicate_candidate_uri_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname, librdf_node *obj)
{
	(void) classname;
	(void) st;

	if(match->priority <= criteria->priority)
	{
		return 0;
	}
	if(match->resource)
	{
		librdf_free_node(match->resource);
	}
	match->resource = librdf_new_node_from_node(obj);
	match->priority = criteria->priority;
	return 1;
}

static int
spindle_predicate_candidate_literal_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname, librdf_node *obj)
{
	char *lang;
	librdf_uri *dturi;
	const char *dtstr;

	lang = librdf_node_get_literal_value_language(obj);
	if(!match->map->datatype)
	{
		/* If there's no datatype specified, match per language */
		return spindle_predicate_candidate_lang_(match, criteria, st, classname, obj, lang);
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
spindle_predicate_candidate_lang_(struct propertymatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, const char *classname, librdf_node *obj, const char *lang)
{
	struct literal_struct *entry, *p;
	size_t c;

	(void) st;
	(void) classname;

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
