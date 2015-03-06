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

static int spindle_doc_modified_(SPINDLECACHE *data);
static int spindle_doc_type_(SPINDLECACHE *data);
static int spindle_doc_label_(SPINDLECACHE *data);
static int spindle_doc_topic_(SPINDLECACHE *data);
static int spindle_doc_score_(SPINDLECACHE *data);

int
spindle_doc_init(SPINDLE *spindle)
{
	spindle->titlepred = twine_config_geta("spindle:predicates:title", NS_RDFS "label");
	return 0;
}

/* Cache information about the document containing the proxy */
int 
spindle_doc_apply(SPINDLECACHE *cache)
{
	if(spindle_doc_modified_(cache))
	{
		return -1;
	}
	if(spindle_doc_topic_(cache))
	{
		return -1;
	}
	if(spindle_doc_type_(cache))
	{
		return -1;
	}
	if(spindle_doc_label_(cache))
	{
		return -1;
	}
	if(spindle_doc_score_(cache))
	{
		return -1;
	}
	return 0;
}

static int
spindle_doc_modified_(SPINDLECACHE *cache)
{
	librdf_node *obj;
	librdf_statement *st;
	char tbuf[64];
	time_t t;
	struct tm now;

	/* Add <doc> dct:modified "now"^^xsd:dateTime */
	t = time(NULL);
	gmtime_r(&t, &now);
	strftime(tbuf, sizeof(tbuf) - 1, "%Y-%m-%dT%H:%M:%SZ", &now);
	st = twine_rdf_st_create();
	if(!st) return -1;
	obj = twine_rdf_node_clone(cache->doc);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_subject(st, obj);
	obj = twine_rdf_node_clone(cache->spindle->modified);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_predicate(st, obj);
	obj = librdf_new_node_from_typed_literal(cache->spindle->world, (const unsigned char *) tbuf, NULL, cache->spindle->xsd_dateTime);
	if(!obj)
	{
		twine_logf(LOG_CRIT, "failed to create new node for \"%s\"^^xsd:dateTime\n");
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_object(st, obj);
	twine_rdf_model_add_st(cache->proxydata, st, cache->graph);
	if(cache->spindle->multigraph)
	{
		/* Also add the statement to the root graph */
		twine_rdf_model_add_st(cache->rootdata, st, cache->spindle->rootgraph);
	}
	twine_rdf_st_destroy(st);
	return 0;
}

static int
spindle_doc_topic_(SPINDLECACHE *cache)
{
	librdf_node *obj;
	librdf_statement *st;

	/* Add a statement stating that <doc> foaf:primaryTopic <self> */
	st = twine_rdf_st_create();
	if(!st) return -1;
	obj = twine_rdf_node_clone(cache->doc);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_subject(st, obj);
	obj = twine_rdf_node_createuri(NS_FOAF "primaryTopic");
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_predicate(st, obj);
	obj = twine_rdf_node_clone(cache->self);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_object(st, obj);
	twine_rdf_model_add_st(cache->proxydata, st, cache->graph);
	if(cache->spindle->multigraph)
	{
		/* Also add the statement to the root graph */
		twine_rdf_model_add_st(cache->rootdata, st, cache->spindle->rootgraph);
	}
	twine_rdf_st_destroy(st);
	return 0;
}

static int
spindle_doc_type_(SPINDLECACHE *cache)
{
	librdf_node *obj;
	librdf_statement *st;

	/* Add a statement stating that <doc> rdf:type foaf:Document */
	st = twine_rdf_st_create();
	if(!st) return -1;
	obj = twine_rdf_node_clone(cache->doc);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_subject(st, obj);
	obj = twine_rdf_node_clone(cache->spindle->rdftype);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_predicate(st, obj);
	obj = twine_rdf_node_createuri(NS_FOAF "Document");
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_object(st, obj);
	twine_rdf_model_add_st(cache->proxydata, st, cache->graph);
	if(cache->spindle->multigraph)
	{
		/* Also add the statement to the root graph */
		twine_rdf_model_add_st(cache->rootdata, st, cache->spindle->rootgraph);
	}
	twine_rdf_st_destroy(st);
	return 0;
}

static int
spindle_doc_label_(SPINDLECACHE *cache)
{
	librdf_node *obj;
	librdf_statement *st;
	char *strbuf;
	const char *s;

	/* Add a statement stating that <doc> rdfs:label "Information about 'foo' */
	if(cache->title_en)
	{
		s = cache->title_en;
	}
	else
	{
		s = cache->title;
	}
	strbuf = (char *) calloc(1, strlen(s) + 32);
	if(!strbuf)
	{
		return -1;
	}
	sprintf(strbuf, "Information about '%s'", s);
	st = twine_rdf_st_create();
	if(!st) return -1;
	obj = twine_rdf_node_clone(cache->doc);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		return -1;
	}
	librdf_statement_set_subject(st, obj);
	obj = twine_rdf_node_createuri(NS_RDFS "label");
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		free(strbuf);
		return -1;
	}
	librdf_statement_set_predicate(st, obj);
	obj = librdf_new_node_from_literal(cache->spindle->world, (const unsigned char *) strbuf, "en", 0);
	if(!obj)
	{
		twine_rdf_st_destroy(st);
		free(strbuf);
		return -1;
	}
	librdf_statement_set_object(st, obj);
	twine_rdf_model_add_st(cache->proxydata, st, cache->graph);
	if(cache->spindle->multigraph)
	{
		/* Also add the statement to the root graph */
		twine_rdf_model_add_st(cache->rootdata, st, cache->spindle->rootgraph);
	}
	twine_rdf_st_destroy(st);
	free(strbuf);
	return 0;
}

static int
spindle_doc_score_(SPINDLECACHE *data)
{
	char scorebuf[64];
	librdf_world *world;
	librdf_statement *st;
	librdf_uri *dturi;
	librdf_node *node;

	if(data->score < 1)
	{
		data->score = 1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": proxy prominence score is %d\n", data->score);
	sprintf(scorebuf, "%d", data->score);
	world = twine_rdf_world();
	st = twine_rdf_st_create();
	librdf_statement_set_subject(st, twine_rdf_node_clone(data->doc));
	librdf_statement_set_predicate(st, twine_rdf_node_createuri(NS_SPINDLE "score"));
	dturi = librdf_new_uri(world, (const unsigned char *) NS_XSD "integer");
	node = librdf_new_node_from_typed_literal(world, (const unsigned char *) scorebuf, NULL, dturi);
	librdf_statement_set_object(st, node);
	/* This information's only added to the root graph */
	twine_rdf_model_add_st(data->rootdata, st, data->spindle->rootgraph);
	twine_rdf_st_destroy(st);
	return 0;
}
