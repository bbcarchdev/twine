/* Twine: SPARQL client helpers
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

#include "p_libtwine.h"

static int twine_sparql_debug;

static char *twine_sparql_query_uri;
static char *twine_sparql_update_uri;
static char *twine_sparql_data_uri;

static int
twine_sparql_put_internal_(const char *uri, const char *triples, size_t length, const char *type, librdf_model *sourcemodel);

/* Internal: set defaults for SPARQL connections */
int
twine_sparql_defaults_(const char *query_uri, const char *update_uri, const char *data_uri, int verbose)
{
	if(query_uri)
	{
		twine_sparql_query_uri = strdup(query_uri);
		if(!twine_sparql_query_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL query endpoint is <%s>\n", twine_sparql_query_uri);
	}
	if(update_uri)
	{
		twine_sparql_update_uri = strdup(update_uri);
		if(!twine_sparql_update_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL update endpoint is <%s>\n", twine_sparql_update_uri);
	}
	if(data_uri)
	{
		twine_sparql_data_uri = strdup(data_uri);
		if(!twine_sparql_data_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL RESTful endpoint is <%s>\n", twine_sparql_data_uri);
	}
	twine_sparql_debug = verbose;
	return 0;
}

/* Public: Create a new SPARQL connection */
SPARQL *
twine_sparql_create(void)
{
	SPARQL *p;

	p = sparql_create(NULL);
	if(!p)
	{
		return NULL;
	}
	sparql_set_logger(p, twine_logger_);
	sparql_set_verbose(p, twine_sparql_debug);
	if(twine_sparql_query_uri)
	{
		sparql_set_query_uri(p, twine_sparql_query_uri);
	}
	if(twine_sparql_update_uri)
	{
		sparql_set_update_uri(p, twine_sparql_update_uri);
	}
	if(twine_sparql_data_uri)
	{
		sparql_set_data_uri(p, twine_sparql_data_uri);
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
	return 0;
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

/* Perform an actual SPARQL PUT operation, dealing with pre-processors and
 * post-processors
 */
static int
twine_sparql_put_internal_(const char *uri, const char *triples, size_t length, const char *type, librdf_model *sourcemodel)
{
	int r, pp;
	SPARQL *conn;
	twine_graph graph;
	char *qbuf, *tbuf;
	size_t l;

	memset(&graph, 0, sizeof(twine_graph));
	graph.uri = uri;
	pp = twine_postproc_registered_() || twine_preproc_registered_();
	conn = twine_sparql_create();
	r = 0;
	if(!conn)
	{
		return -1;
	}
	if(pp)
	{
		/* Obtain 'old' graph if there are any postprocessors registered */
		l = strlen(uri) + 60;
		qbuf = (char *) calloc(1, l + 1);
		if(!qbuf)
		{
			sparql_destroy(conn);
			return -1;
		}
		snprintf(qbuf, l, "SELECT * WHERE { GRAPH <%s> { ?s ?p ?o . } }", uri);
		graph.old = twine_rdf_model_create();
		if(!graph.old)
		{
			sparql_destroy(conn);
			return -1;
		}
		r = sparql_query_model(conn, qbuf, strlen(qbuf), graph.old);
		free(qbuf);
		if(r)
		{
			twine_logf(LOG_ERR, "failed to obtain triples for graph <%s>\n", uri);
			twine_graph_cleanup_(&graph);
			sparql_destroy(conn);
			return -1;
		}
		/* Parse the triples if there are any postprocessors */
		graph.pristine = twine_rdf_model_create();
		if(sourcemodel)
		{
			stream = librdf_model_as_stream(sourcemodel);
			r = librdf_model_add_statements(graph.pristine, model);
			librdf_free_stream(stream);
		}
		else
		{
			r = twine_rdf_model_parse(graph.pristine, type, triples, length);
		}
		if(!r)
		{
			r = twine_preproc_process_(&graph);
		}
	}
	if(!r)
	{
		if(graph.store)
		{
			tbuf = twine_rdf_model_ntriples(graph.store, &l);
			if(tbuf)
			{
				r = sparql_put(conn, uri, tbuf, l);
				librdf_free_memory(tbuf);
			}
			else
			{
				r = -1;
			}
			
		}
		else
		{
			r = sparql_put(conn, uri, triples, length);
		}
	}
	sparql_destroy(conn);
	if(!r && pp)
	{
		twine_postproc_process_(&graph);
	}
	twine_graph_cleanup_(&graph);
	return r;
}
