/* Twine: Workflow processing
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

static int twine_workflow_parse_(TWINE *context, char *str);
static int twine_workflow_config_cb_(const char *key, const char *value, void *data);

/* Built-in workflow processors */
static int twine_workflow_preprocess_(twine_graph *graph, void *dummy);
static int twine_workflow_postprocess_(twine_graph *graph, void *dummy);
static int twine_workflow_sparql_get_(twine_graph *graph, void *dummy);
static int twine_workflow_sparql_put_(twine_graph *graph, void *dummy);

static char **workflow;
static size_t nworkflow;

int
twine_workflow_init_(TWINE *context)
{
	int r;
	char *s;

	if(!context->plugins_enabled)
	{
		return 0;
	}
	twine_plugin_allow_internal_(context, 1);
	twine_graph_register("deprecated:preprocess", twine_workflow_preprocess_, context);
	twine_graph_register("deprecated:postprocess", twine_workflow_postprocess_, context);
	twine_graph_register("sparql-get", twine_workflow_sparql_get_, context);
	twine_graph_register("sparql-put", twine_workflow_sparql_put_, context);
	twine_plugin_allow_internal_(context, 0);
	r = twine_config_get_all("workflow", "invoke", twine_workflow_config_cb_, context);
	if(r < 0)
	{
		return -1;
	}
	if(r)
	{
		if(context->appname && strcmp(context->appname, DEFAULT_CONFIG_SECTION_NAME))
		{
			twine_logf(LOG_NOTICE, "The [workflow] configuration section has been deprecated; you should use workflow=NAME,NAME... in the common [%s] section or the application-specific [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME, context->appname);
		}
		else
		{
			twine_logf(LOG_NOTICE, "The [workflow] configuration section has been deprecated; you should use workflow=NAME,NAME... in the common [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME, context->appname);
		}
		return 0;
	}
	s = twine_config_geta("*:workflow", "");
	if(s)
	{		
		r = twine_workflow_parse_(context, s);
		free(s);
		if(r < 0)
		{
			return -1;
		}
	}
	if(!nworkflow)
	{
		twine_logf(LOG_NOTICE, "no processing workflow was configured; using defaults\n");
		twine_workflow_config_cb_(NULL, "sparql-get", context);
		twine_workflow_config_cb_(NULL, "deprecated:preprocess", context);
		twine_workflow_config_cb_(NULL, "sparql-put", context);
		twine_workflow_config_cb_(NULL, "deprecated:postprocess", context);
	}
	return 0;
}

/* This code is responsible for the processing of a single RDF graph,
 * encapsulated within a twine_graph object, and passing it through
 * whichever modules are configured for graph processing.
 */

int
twine_workflow_process_(twine_graph *graph)
{
	size_t c;
	int r;

	twine_logf(LOG_DEBUG, "workflow: processing <%s>\n", graph->uri);
	r = 0;
	for(c = 0; c < nworkflow; c++)
	{
		twine_logf(LOG_DEBUG, "workflow: invoking graph processor '%s'\n", workflow[c]);
		r = twine_graph_process_(workflow[c], graph);
		if(r)
		{
			break;
		}
	}
	return r;
}

/* Pseudo-processor which in turn invokes any registered pre-processors */
static int
twine_workflow_preprocess_(twine_graph *graph, void *dummy)
{
	(void) dummy;

	return twine_preproc_process_(graph);
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

static int
twine_workflow_parse_(TWINE *context, char *str)
{
	char *p, *s;

	/* Parse a workflow=foo,bar,baz configuration option
	 * Note that processor names can be separated either with whitespace or
	 * with commas or semicolons (or any combination of them), and empty
	 * elements in the list are skipped.
	 */
	for(p = str; *p; p = s)
	{
		while(isspace(*p) || *p == ',' || *p == ';')
		{
			p++;
		}
		if(!*p)
		{
			break;
		}
		for(s = p; *s && !isspace(*s) && *s != ',' && *s != ';'; s++) { }
		if(*s)
		{
			*s = 0;
			s++;
		}
		if(!*p)
		{
			continue;
		}		
		if(twine_workflow_config_cb_(NULL, p, context) < 0)
		{
			return -1;
		}
	}
	return 0;
}

static int
twine_workflow_config_cb_(const char *key, const char *value, void *data)
{
	char **p;

	(void) key;
	(void) data;

	twine_logf(LOG_DEBUG, "adding processor '%s' to workflow\n", value);
	if(!twine_graph_supported(value))
	{
		twine_logf(LOG_CRIT, "graph processor '%s' named in configured workflow is not registered\n", value);
		return -1;
	}
	p = (char **) realloc(workflow, sizeof(char *) * (nworkflow + 2));
	if(!p)
	{
		twine_logf(LOG_CRIT, "failed to expand workflow list\n");
		return -1;
	}
	workflow = p;
	p[nworkflow] = strdup(value);
	if(!p[nworkflow])
	{
		twine_logf(LOG_CRIT, "failed to duplicate graph processor name while adding to workflow\n");
		return -1;
	}
	nworkflow++;
	p[nworkflow] = NULL;
	return 0;
}
