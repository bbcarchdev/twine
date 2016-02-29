/* Twine: SPARQL client helpers
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

#include "p_libtwine.h"

static int
twine_sparql_put_internal_(const char *uri, const char *triples, size_t length, const char *type, librdf_model *sourcemodel);

/* Internal API: set configuration for SPARQL connections
 *
 * Note that this will have no effect on SPARQL connection objects which
 * have been created prior to this call.
 */
int
twine_set_sparql(TWINE *restrict context, const char *base_uri, const char *query_uri, const char *update_uri, const char *data_uri, int verbose)
{
	if(base_uri)
	{
		context->sparql_uri = strdup(base_uri);
		if(!context->sparql_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL endpoint is <%s>\n", context->sparql_uri);
	}
	if(query_uri)
	{
		context->sparql_query_uri = strdup(query_uri);
		if(!context->sparql_query_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL query endpoint is <%s>\n", context->sparql_query_uri);
	}
	if(update_uri)
	{
		context->sparql_update_uri = strdup(update_uri);
		if(!context->sparql_update_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL update endpoint is <%s>\n", context->sparql_update_uri);
	}
	if(data_uri)
	{
		context->sparql_data_uri = strdup(data_uri);
		if(!context->sparql_data_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL RESTful endpoint is <%s>\n", context->sparql_data_uri);
	}
	context->sparql_debug = verbose;
	return 0;
}

/* Public: Create a new SPARQL connection */
SPARQL *
twine_sparql_create(void)
{
	SPARQL *p;

	p = sparql_create(twine_->sparql_uri);
	if(!p)
	{
		twine_logf(LOG_CRIT, "failed to create new SPARQL connection\n");
		return NULL;
	}
	sparql_set_logger(p, twine_->logger);
	sparql_set_verbose(p, twine_->sparql_debug);
	if(twine_->sparql_query_uri)
	{
		sparql_set_query_uri(p, twine_->sparql_query_uri);
	}
	if(twine_->sparql_update_uri)
	{
		sparql_set_update_uri(p, twine_->sparql_update_uri);
	}
	if(twine_->sparql_data_uri)
	{
		sparql_set_data_uri(p, twine_->sparql_data_uri);
	}
	return p;
}

/* Public: Replace a graph from a Turtle buffer */
int
twine_sparql_put(const char *uri, const char *triples, size_t length)
{
	return twine_sparql_put_internal_(uri, triples, length, "text/turtle", NULL);
}

int
twine_sparql_put_format(const char *uri, const char *triples, size_t length, const char *type)
{
	return twine_sparql_put_internal_(uri, triples, length, type, NULL);
}

/* Public: Replace a graph from a librdf stream */
int
twine_sparql_put_stream(const char *uri, librdf_stream *stream)
{
	librdf_model *model;
	char *buf;
	size_t buflen;
	int r;

	model = twine_rdf_model_create();
	if(!model)
	{
		return -1;
	}
	if(librdf_model_add_statements(model, stream))
	{
		return -1;
	}
	buf = twine_rdf_model_ntriples(model, &buflen);
	if(!buf)
	{
		return -1;
	}
	r = twine_sparql_put_internal_(uri, buf, buflen, MIME_NTRIPLES, model);
	librdf_free_memory(buf);
	twine_rdf_model_destroy(model);
	return r;
}

/* Public: Replace a graph from a librdf model */
int
twine_sparql_put_model(const char *uri, librdf_model *model)
{
	char *buf;
	size_t buflen;
	int r;

	buf = twine_rdf_model_ntriples(model, &buflen);
	if(!buf)
	{
		return -1;
	}
	r = twine_sparql_put_internal_(uri, buf, buflen, MIME_NTRIPLES, model);
	librdf_free_memory(buf);
	return r;
}

/* Construct an RDF graph and pass it to the processing modules which
 * constitute the workflow.
 */
static int
twine_sparql_put_internal_(const char *uri, const char *triples, size_t length, const char *type, librdf_model *sourcemodel)
{
	int r;
	twine_graph graph;
	librdf_stream *stream;

	memset(&graph, 0, sizeof(twine_graph));
	graph.uri = uri;
	graph.store = twine_rdf_model_create();
	if(sourcemodel)
	{
		stream = librdf_model_as_stream(sourcemodel);
		r = librdf_model_add_statements(graph.store, stream);
		librdf_free_stream(stream);
	}
	else
	{
		r = twine_rdf_model_parse(graph.store, type, triples, length);
	}
	if(!r)
	{
		r = twine_workflow_process_(&graph);
	}
	twine_graph_cleanup_(&graph);
	return r;
}
