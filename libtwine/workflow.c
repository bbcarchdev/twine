/* Twine: Workflow processing
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

/* Built-in workflow processors */
static int twine_workflow_preprocess_(twine_graph *graph, void *dummy);
static int twine_workflow_postprocess_(twine_graph *graph, void *dummy);
static int twine_workflow_sparql_get_(twine_graph *graph, void *dummy);
static int twine_workflow_sparql_put_(twine_graph *graph, void *dummy);

int
twine_workflow_init_(void)
{
	twine_plugin_internal_(1);
	twine_graph_register("preprocess", twine_workflow_preprocess_, NULL);
	twine_graph_register("postprocess", twine_workflow_postprocess_, NULL);
	twine_graph_register("sparql-get", twine_workflow_sparql_get_, NULL);
	twine_graph_register("sparql-put", twine_workflow_sparql_put_, NULL);
	twine_plugin_internal_(0);
	return 0;
}

/* This code is responsible for the processing of a single RDF graph,
 * encapsulated within a twine_graph object, and passing it through
 * whichever modules are configured for graph processing.
 */

int
twine_workflow_process_(twine_graph *graph)
{
	/* The workflow is not yet completely configurable, and so we follow
	 * a fixed path here:
	 *
	 * - Invoke sparql-get to obtain the 'old' graph (if any)
	 * - Invoke preprocessors
	 * - Invoke sparql-put to push the 'new' graph
	 * - Invoke postprocessors
	 */
	if(twine_workflow_sparql_get_(graph, NULL))
	{
		return -1;
	}
	if(twine_workflow_preprocess_(graph, NULL))
	{
		return -1;
	}
	if(twine_workflow_sparql_put_(graph, NULL))
	{
		return -1;
	}
	if(twine_workflow_postprocess_(graph, NULL))
	{
		return -1;
	}
	return 0;
}

/* Pseudo-processor which in turn invokes any registered pre-processors */
static int
twine_workflow_preprocess_(twine_graph *graph, void *dummy)
{
	(void) dummy;

	return twine_postproc_process_(graph);
}

/* Pseudo-processor which in turn invokes any registered post-processors */
static int
twine_workflow_postprocess_(twine_graph *graph, void *dummy)
{
	(void) dummy;

	return twine_postproc_process_(graph);
}

/* Obtain any previously-stored version of an RDF graph */
static int
twine_workflow_sparql_get_(twine_graph *graph, void *dummy)
{
	char *qbuf;
	size_t l;
	SPARQL *conn;
	int r;

	(void) dummy;

	conn = twine_sparql_create();
	l = strlen(graph->uri) + 60;
	qbuf = (char *) calloc(1, l + 1);
	if(!qbuf)
	{
		sparql_destroy(conn);
		return -1;
	}
	snprintf(qbuf, l, "SELECT * WHERE { GRAPH <%s> { ?s ?p ?o . } }", graph->uri);
	graph->old = twine_rdf_model_create();
	if(!graph->old)
	{
		free(qbuf);
		sparql_destroy(conn);
		return -1;
	}
	r = sparql_query_model(conn, qbuf, strlen(qbuf), graph->old);
	free(qbuf);
	if(r)
	{
		twine_logf(LOG_ERR, "failed to obtain triples for graph <%s>\n", graph->uri);
		sparql_destroy(conn);
		return -1;
	}
	sparql_destroy(conn);
	return 0;
}

/* Push a new, possibly-modified, version of an RDF graph into a quad-store */
static int
twine_workflow_sparql_put_(twine_graph *graph, void *dummy)
{
	SPARQL *conn;
	size_t l;
	char *tbuf;
	int r;

	(void) dummy;

	conn = twine_sparql_create();
	tbuf = twine_rdf_model_ntriples(graph->store, &l);
	if(tbuf)
	{
		r = sparql_put(conn, graph->uri, tbuf, l);
		librdf_free_memory(tbuf);
	}
	else
	{
		r = -1;
	}
	sparql_destroy(conn);
	return 0;
}
