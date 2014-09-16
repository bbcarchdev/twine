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

/* A single predicate which should be matched; optionally matching is restricted
 * to members of a particular class (the class must be defined in classes.c)
 * Priority values are 0 for 'always add', or 1..n, where 1 is highest-priority.
 */
struct predicatematch_struct
{
	int priority;
	const char *predicate;
	const char *onlyfor;
};

/* Mapping data for a predicate. 'target' is the predicate which should be
 * used in the proxy data. If 'expected' is RAPTOR_TERM_TYPE_LITERAL, then
 * 'datatype' can optionally specify a datatype which literals must conform
 * to (candidate literals must either have no datatype and language, or
 * be of the specified datatype).
 *
 * If 'expected' is RAPTOR_TERM_TYPE_URI and proxyonly is nonzero, then
 * only those candidate properties whose objects have existing proxy
 * objects within the store will be used (and the triple stored in the
 * proxy will point to the corresponding proxy instead of the original
 * URI).
 */
struct predicatemap_struct
{
	const char *target;
	struct predicatematch_struct *matches;
	raptor_term_type expected;
	const char *datatype;
	int proxyonly;
};

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
	struct predicatemap_struct *map;
	int priority;
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
	{ 30, "http://www.tate.org.uk/ontologies/collection#fc", "http://xmlns.com/foaf/0.1/Person" },
	{ 35, "http://www.tate.org.uk/ontologies/collection#mda", "http://xmlns.com/foaf/0.1/Person" },
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
	{ 50, "http://www.w3.org/2003/01/geo/wgs84_pos#lat", "http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing" },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct long_match[] = {
	{ 50, "http://www.w3.org/2003/01/geo/wgs84_pos#long", "http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing" },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct depiction_match[] = {
	{ 0, "http://xmlns.com/foaf/0.1/depiction", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/thumbnail", NULL },
	{ 0, "http://www.tate.org.uk/ontologies/collection#thumbnailUrl", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct subject_match[] = {
	{ 0, "http://xmlns.com/foaf/0.1/topic", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/primaryTopic", NULL },
	{ 0, "http://purl.org/dc/terms/subject", NULL },
	{ 0, "http://purl.org/dc/elements/1.1/subject", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct inscheme_match[] = {
	{ 0, "http://www.w3.org/2004/02/skos/core#inScheme", NULL },
	{ 0, "http://www.w3.org/2008/05/skos#inScheme", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct broader_match[] = {
	{ 0, "http://www.w3.org/2004/02/skos/core#broader", NULL },
	{ 0, "http://www.w3.org/2008/05/skos#broader", NULL },
	{ 0, "http://www.tate.org.uk/ontologies/collection#parentSubject", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct narrower_match[] = {
	{ 0, "http://www.w3.org/2004/02/skos/core#narrower", NULL },
	{ 0, "http://www.w3.org/2008/05/skos#narrower", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct locatedin_match[] = {
	{ 0, "http://www.geonames.org/ontology#locatedIn", NULL },
	{ 0, "http://www.tate.org.uk/ontologies/collection#place", "http://purl.org/vocab/frbr/core#Work" },
	{ 0, "http://www.geonames.org/ontology#parentCountry", NULL },
	{ 0, "http://www.geonames.org/ontology#parentFeature", NULL },
	{ 0, "http://www.geonames.org/ontology#parentADM1", NULL },
	{ 0, "http://www.geonames.org/ontology#parentADM2", NULL },
	{ 0, "http://www.geonames.org/ontology#parentADM3", NULL },
	{ 0, "http://www.geonames.org/ontology#parentADM4", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct part_match[] = {
	{ 0, "http://purl.org/dc/terms/isPartOf", NULL },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct place_match[] = {
	{ 0, "http://purl.org/NET/c4dm/event.owl#place", "http://purl.org/NET/c4dm/event.owl#Event" },
	{ 0, "http://www.tate.org.uk/ontologies/collection#place", "http://purl.org/NET/c4dm/event.owl#Event" },
	{ -1, NULL, NULL }
};

static struct predicatematch_struct page_match[] = {
	{ 0, "http://xmlns.com/foaf/0.1/page", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/homepage", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/weblog", NULL },
	{ 0, "http://www.geonames.org/ontology#wikipediaArticle", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/workInfoHomepage", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/workplaceHomepage", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/schoolHomepage", NULL },
	{ 0, "http://xmlns.com/foaf/0.1/isPrimaryTopicOf", NULL },
	{ -1, NULL, NULL }
};

static struct predicatemap_struct predicatemap[] = {
	{
		"http://www.w3.org/2000/01/rdf-schema#label",
		label_match,
		RAPTOR_TERM_TYPE_LITERAL,
		NULL,
		-1,
	},
	{
		"http://purl.org/dc/terms/description",
		description_match,
		RAPTOR_TERM_TYPE_LITERAL,
		NULL,
		-1,
	},
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#lat",
		lat_match,
		RAPTOR_TERM_TYPE_LITERAL,
		"http://www.w3.org/2001/XMLSchema#decimal",
		-1,
	},
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#long",
		long_match,
		RAPTOR_TERM_TYPE_LITERAL,
		"http://www.w3.org/2001/XMLSchema#decimal",
		-1,
	},
	{
		"http://xmlns.com/foaf/0.1/depiction",
		depiction_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		0
	},
	{
		"http://www.w3.org/2004/02/skos/core#inScheme",
		inscheme_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		1
	},
	{
		"http://www.w3.org/2004/02/skos/core#broader",
		broader_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		1
	},
	{
		"http://www.w3.org/2004/02/skos/core#narrower",
		narrower_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		1
	},
	{
		"http://xmlns.com/foaf/0.1/topic",
		subject_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		1
	},
	{
		"http://purl.org/dc/terms/isPartOf",
		part_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		1
	},
	{
		"http://www.geonames.org/ontology#locatedIn",
		locatedin_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		1
	},
	{
		"http://purl.org/NET/c4dm/event.owl#place",
		place_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		1
	},
	{
		"http://xmlns.com/foaf/0.1/page",
		page_match,
		RAPTOR_TERM_TYPE_URI,
		NULL,
		0
	},
	{ NULL, NULL, RAPTOR_TERM_TYPE_UNKNOWN, NULL, -1 }
};

static int spindle_prop_init_(struct propdata_struct *data, SPINDLECACHE *cache);
static int spindle_prop_cleanup_(struct propdata_struct *data);
static int spindle_prop_modified_(struct propdata_struct *data);
static int spindle_prop_loop_(struct propdata_struct *data);
static int spindle_prop_test_(struct propdata_struct *data, librdf_statement *st, const char *predicate);
static int spindle_prop_candidate_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_uri_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_literal_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj);
static int spindle_prop_candidate_lang_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj, const char *lang);

static int spindle_prop_apply_(struct propdata_struct *data);

int
spindle_prop_update(SPINDLECACHE *cache)
{
	struct propdata_struct data;
	int r;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": updating properties for <%s>\n", cache->localname);
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
	data->context = cache->graph;
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
	librdf_model_context_add_statement(data->proxymodel, data->context, st);
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
				lpst = twine_rdf_st_clone(pst);
				if(!lpst)
				{
					r = -1;
					break;
				}
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
		librdf_model_context_add_statement(data->proxymodel, data->context, newst);
		twine_rdf_st_destroy(newst);
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
spindle_prop_candidate_literal_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj)
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
		return 1;
	}
	return 0;
}

static int
spindle_prop_candidate_lang_(struct propdata_struct *data, struct propmatch_struct *match, struct predicatematch_struct *criteria, librdf_statement *st, librdf_node *obj, const char *lang)
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
	node = twine_rdf_node_clone(obj);
	if(!node)
	{
		return -1;
	}
	twine_rdf_node_destroy(entry->node);
	entry->node = node;
	entry->priority = criteria->priority;
	return 1;
}
