/* Twine: RDF (quad) processing
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libtwine.h"

#define TWINE_PLUGIN_NAME               "rdf"

static int process_rdf(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, const char *restrict subject, void *data);
static int dump_nquads(TWINE *restrict context, TWINEGRAPH *restrict graph, void *data);
static int describe_sources(librdf_model *model);

/* Twine plug-in entry-point */
int
twine_entry(TWINE *context, TWINEENTRYTYPE type, void *handle)
{
	(void) handle;

	switch(type)
	{
	case TWINE_ATTACHED:
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME " plug-in: initialising\n");
		twine_plugin_add_input(context, "application/trig", "RDF TriG", process_rdf, NULL);
		twine_plugin_add_input(context, "application/n-quads", "RDF N-Quads", process_rdf, NULL);
		twine_plugin_add_input(context, "text/x-nquads", "RDF N-Quads", process_rdf, NULL);
		twine_plugin_add_processor(context, "dump-nquads", dump_nquads, NULL);
		break;
	case TWINE_DETACHED:
		break;
	}
	return 0;
}

/* Parse some supported RDF quads serialisation and trigger processing of
 * the resulting graphs.
 *
 * Although in principle this processor can handle anything that librdf can
 * parse, it will do nothing unless there are named graphs present, and so
 * only N-Quads and TriG MIME types are registered.
 */

static int
process_rdf(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, const char *restrict subject, void *data)
{
	librdf_model *model;
	librdf_iterator *iter;
	librdf_node *node;
	librdf_uri *uri;
	librdf_stream *stream;	
	int r;

	(void) subject;
	(void) data;

	r = 0;

	// Create the model
	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_ERR, "failed to create new RDF model\n");
		return -1;
	}

	// Parse the data
	twine_logf(LOG_DEBUG, "parsing buffer into model as '%s'\n", mime);
	if(twine_rdf_model_parse(model, mime, (const char *) buf, buflen))
	{
		twine_logf(LOG_ERR, "failed to parse string into model\n");
		twine_rdf_model_destroy(model);
		return -1;
	}

	// Verify that we have contexts to work with
	iter = librdf_model_get_contexts(model);
	if(!iter)
	{
		twine_logf(LOG_ERR, "failed to retrieve contexts from model\n");
		twine_rdf_model_destroy(model);
		return -1;
	}
	if(librdf_iterator_end(iter))
	{
		twine_logf(LOG_ERR, "model contains no named graphs\n");
		librdf_free_iterator(iter);
		twine_rdf_model_destroy(model);
		return -1;
	}

	// Add some extra triples to describe the root of the contexts
	// this is later used by the membership indexing to describe the
	// data as being part of a collection. This will create new contexts
	describe_sources(model);
	librdf_free_iterator(iter);
	iter = librdf_model_get_contexts(model);

	// Process all the contexts
	while(!librdf_iterator_end(iter))
	{
		node = (librdf_node *) librdf_iterator_get_object(iter);
		if(!node)
		{
			continue;
		}
		else if(librdf_node_is_resource(node))
		{
			uri = librdf_node_get_uri(node);
			stream = librdf_model_context_as_stream(model, node);
			twine_logf(LOG_DEBUG, "RDF: processing graph <%s>\n", (const char *) librdf_uri_as_string(uri));
			if(twine_workflow_process_stream(context, (const char *) librdf_uri_as_string(uri), stream))
			{
				twine_logf(LOG_ERR, "failed to process graph <%s>\n", (const char *) librdf_uri_as_string(uri));
				r = 1;
				break;
			}
			librdf_free_stream(stream);
		}
		librdf_iterator_next(iter);
	}

	librdf_free_iterator(iter);
	twine_rdf_model_destroy(model);

	return r;
}

/* Graph processor which simply outputs the contents of the graph as N-Quads,
 * for debugging or conversion purposes. The serialised quads are written to
 * standard output.
 */
static int
dump_nquads(TWINE *restrict context, TWINEGRAPH *restrict graph, void *data)
{
	char *quads;
	size_t quadlen;
	
	(void) context;
	(void) data;

	quads = twine_rdf_model_nquads(twine_graph_model(graph), &quadlen);
	if(!quads)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to generate N-Quads for <%s>\n", twine_graph_uri(graph));
		return -1;
	}
	fwrite(quads, quadlen, 1, stdout);
	librdf_free_memory(quads);
	return 0;
}

/*
 * This function looks into the different contexts available in the Quads and
 * describe their corresponding root. For example when a context is found for
 * 'http://dbpedia.org/data/Cardiff.xml' the following triples will be added:
 * <http://dbpedia.org/> a void:Dataset.
 * <http://dbpedia.org/> rdfs:label "Data from 'dbpedia.org'"@en.
 *
 * This will later be useful by Spindle that will add the resources as being
 * members of that root URI. In Quilt that will ultimately show up as part of
 * the collections.
 */
static int
describe_sources(librdf_model *model)
{
	librdf_iterator *iter;
	librdf_node *node;
	librdf_uri *nodeuri;
	const char *nodeuristr;
	librdf_statement *statement;
	const char *base_template = "http://%s/";
	const char *literal_template = "Data from '%s'";
	char *strbuf, *hostname, *baseuri;
	size_t i;

	// Get all the sources
	iter = librdf_model_get_contexts(model);
	for(; !librdf_iterator_end(iter); librdf_iterator_next(iter))
	{
		// Extract the needed resources
		node = librdf_iterator_get_object(iter);
		nodeuri = librdf_node_get_uri(node);
		nodeuristr = (const char *)librdf_uri_as_string(nodeuri);
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": uristr {%s} \n", nodeuristr);
		hostname = strtok(nodeuristr, "/");
		hostname = strtok(NULL, "/");
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": hostname {%s} \n", hostname);
		baseuri =  (char *) calloc(1, strlen(hostname) + strlen(base_template) + 1);
		sprintf(baseuri, base_template, hostname);
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": base {%s} \n", baseuri);

		// Say that we have a void:Dataset
		statement = twine_rdf_st_create();
		librdf_statement_set_subject(statement, twine_rdf_node_createuri(baseuri));
		librdf_statement_set_predicate(statement, twine_rdf_node_createuri("http://www.w3.org/1999/02/22-rdf-syntax-ns#type"));
		librdf_statement_set_object(statement, twine_rdf_node_createuri("http://rdfs.org/ns/void#Dataset"));
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": adding {%s} to {%s}\n", librdf_statement_to_string(statement), baseuri);
		twine_rdf_model_add_st(model, statement, twine_rdf_node_createuri(baseuri));
		librdf_free_statement(statement);

		// Give it a label
		statement = twine_rdf_st_create();
		librdf_statement_set_subject(statement, twine_rdf_node_createuri(baseuri));
		librdf_statement_set_predicate(statement, twine_rdf_node_createuri("http://www.w3.org/2000/01/rdf-schema#label"));
		strbuf = (char *) calloc(1, strlen(hostname) + strlen(literal_template) + 1);
		sprintf(strbuf, literal_template, hostname);
		librdf_statement_set_object(statement, twine_rdf_node_createliteral(strbuf));
		free(strbuf);
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": adding {%s} to {%s}\n", librdf_statement_to_string(statement), baseuri);
		twine_rdf_model_add_st(model, statement, twine_rdf_node_createuri(baseuri));
		librdf_free_statement(statement);

		free(baseuri);
	}
	librdf_free_iterator(iter);

	return 0;
}

