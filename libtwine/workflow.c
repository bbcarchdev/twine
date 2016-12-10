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
static int twine_workflow_process_single_(TWINE *context, TWINEGRAPH *graph, const char *name);

/* Built-in workflow processors */
static int twine_workflow_preprocess_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy);
static int twine_workflow_postprocess_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy);
static int twine_workflow_sparql_get_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy);
static int twine_workflow_sparql_put_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy);
static int twine_workflow_s3_get_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy);
static int twine_workflow_s3_put_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy);

/* Utility function for interfacing with S3 and the DB */
static int twine_cache_store_s3_(const char *g, char *ntbuf, size_t bufsize);
static int twine_cache_fetch_s3_(const char *g, char **ntbuf, size_t *buflen);
static size_t twine_cache_store_s3_upload_(char *buffer, size_t size, size_t nitems, void *userdata);
static size_t twine_cache_store_s3_download_(char *buffer, size_t size, size_t nitems, void *userdata);

static char **workflow;
static size_t nworkflow;

struct s3_upload_struct
{
	char *buf;
	size_t bufsize;
	size_t pos;
};

/* Public: process a single message, passing it to whatever input handler
 * supports messages of the specified MIME type */
int
twine_workflow_process_message(TWINE *restrict context, const char *restrict mimetype, const unsigned char *restrict message, size_t messagelen, const char *restrict subject)
{
	size_t l, tl;
	const char *s;
	void *prev;
	int r;

	s = strchr(mimetype, ';');
	if(s)
	{
		tl = s - mimetype;
	}
	else
	{
		tl = strlen(mimetype);
	}
	prev = context->plugin_current;
	for(l = 0; l < context->cbcount; l++)
	{
		if(context->callbacks[l].type == TCB_INPUT &&
		   !strncasecmp(context->callbacks[l].m.input.type, mimetype, tl) &&
		   !context->callbacks[l].m.input.type[tl])
		{
			context->plugin_current = context->callbacks[l].module;
			r = context->callbacks[l].m.input.fn(context, mimetype, message, messagelen, subject, context->callbacks[l].data);
			context->plugin_current = prev;
			return r;
		}	
		if(context->callbacks[l].type == TCB_LEGACY_MIME &&
		   !strncasecmp(context->callbacks[l].m.legacy_mime.type, mimetype, tl) &&
		   !context->callbacks[l].m.legacy_mime.type[tl])
		{
			context->plugin_current = context->callbacks[l].module;
			r = context->callbacks[l].m.legacy_mime.fn(mimetype, message, messagelen, context->callbacks[l].data);
			context->plugin_current = prev;
			return r;
		}
	}
	twine_logf(LOG_ERR, "no available input handler for messages of type '%s'\n", mimetype);
	return -1;	
}

/* Public: process a file via a registered bulk-import mechanism */
int
twine_workflow_process_file(TWINE *restrict context, const char *restrict mimetype, FILE *restrict file)
{
	struct twine_callback_struct *importer;
	void *prev;
	unsigned char *buffer;
	const unsigned char *p;
	size_t l, bufsize, buflen;
	ssize_t r;
	
	prev = context->plugin_current;
	importer = NULL;
	for(l = 0; l < context->cbcount; l++)
	{
		if(context->callbacks[l].type == TCB_BULK &&
		   !strcmp(context->callbacks[l].m.bulk.type, mimetype))
		{
			importer = &(context->callbacks[l]);
			break;
		}
		if(context->callbacks[l].type == TCB_LEGACY_BULK &&
		   !strcmp(context->callbacks[l].m.legacy_bulk.type, mimetype))
		{
			importer = &(context->callbacks[l]);
			break;
		}
	}
	if(!importer)
	{
		twine_logf(LOG_ERR, "no bulk importer registered for '%s'\n", mimetype);
		return -1;
	}
	buffer = NULL;
	bufsize = 0;
	buflen = 0;
	context->plugin_current = importer->module;
	while(!feof(file))
	{
		if(bufsize - buflen < 1024)
		{
			p = (unsigned char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				twine_logf(LOG_CRIT, "failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				free(buffer);
				context->plugin_current = prev;
				return -1;
			}
			buffer = (unsigned char *) p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, file);
		if(r < 0)
		{
			twine_logf(LOG_CRIT, "I/O error during bulk import: %s\n", strerror(errno));
			free(buffer);
			context->plugin_current = prev;
			return -1;
		}
		buflen += r;
		buffer[buflen] = 0;
		if(!buflen)
		{
			/* Nothing new was read */
			continue;
		}
		if(importer->type == TCB_BULK)
		{
			p = importer->m.bulk.fn(context, mimetype, buffer, buflen, importer->data);
		}
		else
		{
			/* Legacy callback */
			p = importer->m.legacy_bulk.fn(importer->m.legacy_bulk.type, buffer, buflen, importer->data);
		}
		if(!p)
		{
			twine_logf(LOG_ERR, "bulk importer failed\n");
			free(buffer);
			context->plugin_current = prev;
			return -1;
		}
		if(p == buffer)
		{
			continue;
		}
		if(p < buffer || p > buffer + buflen)
		{
			twine_logf(LOG_ERR, "bulk importer returned a buffer pointer out of bounds\n");
			free(buffer);
			context->plugin_current = prev;
			return -1;
		}
		l = buflen - (p - buffer);
		memmove(buffer, p, l);
		buflen = l;
	}
	if(buflen)
	{
		if(importer->type == TCB_BULK)
		{			
			p = importer->m.bulk.fn(twine_, mimetype, buffer, buflen, importer->data);
		}
		else
		{
			p = importer->m.legacy_bulk.fn(importer->m.legacy_bulk.type, buffer, buflen, importer->data);
		}
		if(!p)
		{
			twine_logf(LOG_ERR, "bulk importer failed\n");
			free(buffer);
			context->plugin_current = prev;
			return -1;			
		}
	}
	if(importer->type == TCB_BULK)
	{
		/* Send a zero-length update to signal the end of the bulk import
		 * This behaviour is not applied to legacy context->callbacks
		 */
		importer->m.bulk.fn(twine_, mimetype, buffer, 0, importer->data);
	}
	context->plugin_current = prev;
	free(buffer);
	return 0;	
}

/* Public: process a graph object */
int
twine_workflow_process_graph(TWINE *restrict context, TWINEGRAPH *restrict graph)
{
	size_t c;
	int r;

	twine_logf(LOG_DEBUG, "workflow: processing <%s>\n", graph->uri);
	r = 0;
	for(c = 0; c < nworkflow; c++)
	{
		twine_logf(LOG_DEBUG, "workflow: invoking graph processor '%s'\n", workflow[c]);
		r = twine_workflow_process_single_(context, graph, workflow[c]);
		if(r)
		{
			break;
		}
	}
	return r;
}

/* Public: process an update instruction */
int
twine_workflow_process_update(TWINE *restrict context, const char *restrict type, const char *restrict id)
{
	struct twine_callback_struct *plugin;
	void *prev;
	size_t l;
	int r;
	
	prev = context->plugin_current;
	plugin = NULL;
	for(l = 0; l < context->cbcount; l++)
	{
		if(context->callbacks[l].type == TCB_UPDATE &&
		   !strcasecmp(context->callbacks[l].m.update.name, type))
		{
			plugin = &(context->callbacks[l]);
			break;
		}
		if(context->callbacks[l].type == TCB_LEGACY_UPDATE &&
		   !strcasecmp(context->callbacks[l].m.legacy_update.name, type))
		{
			plugin = &(context->callbacks[l]);
			break;
		}
	}
	if(!plugin)
	{
		twine_logf(LOG_ERR, "no update handler '%s' has been registered\n", type);
		return -1;
	}
	context->plugin_current = plugin->module;
	if(plugin->type == TCB_UPDATE)
	{
		r = plugin->m.update.fn(context, plugin->m.legacy_update.name, id, plugin->data);
	}
	else
	{
		/* Legacy update callback */
		r = plugin->m.legacy_update.fn(plugin->m.legacy_update.name, id, plugin->data);
	}
	context->plugin_current = prev;
	if(r)
	{
		twine_logf(LOG_ERR, "handler '%s' failed to update <%s>\n", plugin->m.legacy_update.name, id);
		return -1;
	}
	return 0;
}


/* Public: process a set of RDF triples (by creating a graph and then invoking
 * twine_workflow_process_graph() on it)
 */
int
twine_workflow_process_rdf(TWINE *restrict context, const char *restrict uri, const unsigned char *restrict buf, size_t buflen, const char *restrict type)
{
	TWINEGRAPH *g;
	int r;

	g = twine_graph_create_rdf(context, uri, buf, buflen, type);
	if(!g)
	{
		return -1;
	}
	r = twine_workflow_process_graph(context, g);
	twine_graph_destroy(g);
	return r;
}

/* Public: process a set of RDF triples from a graph, provided in the form
 * of a librdf_stream
 */
int
twine_workflow_process_stream(TWINE *restrict context, const char *restrict uri, librdf_stream *stream)
{
	TWINEGRAPH *g;
	int r;
	librdf_node *node;

	node = twine_rdf_node_createuri(uri);
	if(!node)
	{
		return -1;
	}
	g = twine_graph_create(context, uri);
	if(!g)
	{
		twine_rdf_node_destroy(node);
		return -1;
	}
	if(librdf_model_context_add_statements(g->store, node, stream))
	{
		twine_logf(LOG_ERR, "failed to add statements to graph <%s>\n", uri);
		twine_rdf_node_destroy(node);
		twine_graph_destroy(g);
		return -1;
	}
	twine_rdf_node_destroy(node);
	r = twine_workflow_process_graph(context, g);
	twine_graph_destroy(g);
	return r;
}

/* Private: initialise workflow processing on a context */
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
	twine_plugin_add_processor(context, "deprecated:preprocess", twine_workflow_preprocess_, context);
	twine_plugin_add_processor(context, "deprecated:postprocess", twine_workflow_postprocess_, context);
	twine_plugin_add_processor(context, "sparql-get", twine_workflow_sparql_get_, context);
	twine_plugin_add_processor(context, "sparql-put", twine_workflow_sparql_put_, context);
	twine_plugin_add_processor(context, "s3-get", twine_workflow_s3_get_, context);
	twine_plugin_add_processor(context, "s3-put", twine_workflow_s3_put_, context);
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

/* 'builtin:preprocess' processor: Pseudo-processor which in turn invokes any
 * registered (legacy) pre-processors
 */
static int
twine_workflow_preprocess_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy)
{
	void *prev;
	size_t c;
	int r;
	
	(void) dummy;

	twine_logf(LOG_DEBUG, "invoking pre-processors for <%s>\n", graph->uri);
	prev = context->plugin_current;
	r = 0;
	for(c = 0; c < context->cbcount; c++)
	{
		if(context->callbacks[c].type == TCB_PROCESSOR &&
		   !strncmp(context->callbacks[c].m.processor.name, "pre:", 4))
		{
			context->plugin_current = context->callbacks[c].module;
			if(context->callbacks[c].m.processor.fn(context, graph, context->callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", context->callbacks[c].m.processor.name);
				r = -1;
				break;
			}
		}
		if(context->callbacks[c].type == TCB_LEGACY_GRAPH &&
		   !strncmp(context->callbacks[c].m.legacy_graph.name, "pre:", 4))
		{
			context->plugin_current = context->callbacks[c].module;
			if(context->callbacks[c].m.legacy_graph.fn(graph, context->callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", context->callbacks[c].m.legacy_graph.name);
				r = -1;
				break;
			}
		}
	}
	context->plugin_current = prev;
	return r;
}

/* 'builtin:postprocess' processor: Pseudo-processor which in turn invokes
 * any registered (legacy) post-processors */
static int
twine_workflow_postprocess_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy)
{
	size_t c;
	void *prev;
	int r;

	(void) dummy;

	twine_logf(LOG_DEBUG, "invoking post-processors for <%s>\n", graph->uri);
	prev = context->plugin_current;
	r = 0;
	for(c = 0; c < context->cbcount; c++)
	{
		if(context->callbacks[c].type == TCB_PROCESSOR &&
		   !strncmp(context->callbacks[c].m.processor.name, "post:", 5))
		{
			context->plugin_current = context->callbacks[c].module;
			if(context->callbacks[c].m.processor.fn(context, graph, context->callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", context->callbacks[c].m.processor.name);
				r = -1;
				break;
			}
		}
		if(context->callbacks[c].type == TCB_LEGACY_GRAPH &&
		   !strncmp(context->callbacks[c].m.legacy_graph.name, "post:", 5))
		{
			context->plugin_current = context->callbacks[c].module;
			if(context->callbacks[c].m.legacy_graph.fn(graph, context->callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", context->callbacks[c].m.legacy_graph.name);
				r = -1;
				break;
			}
		}
	}
	context->plugin_current = prev;
	return r;
}

/* 'sparql-get' processor: Obtain any previously-stored version of an RDF
 * graph from the configured SPARQL store
 */
static int
twine_workflow_sparql_get_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy)
{
	char *qbuf;
	size_t l;
	SPARQL *conn;
	int r;

	(void) context;
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

/* 'sparql-put' processor: Push a new, possibly-modified, version of an RDF graph into a quad-store */
static int
twine_workflow_sparql_put_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy)
{
	SPARQL *conn;
	size_t l;
	char *tbuf;
	int r;

	(void) context;
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
	return r;
}

/* 's3-get' processor: save an RDF graph to the cache of twine and index it */
static int
twine_workflow_s3_get_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy)
{
	size_t l;
	char *tbuf;

	(void) context;
	(void) dummy;

	twine_logf(LOG_DEBUG, "S3 Get\n");

	// Get the triples as a buffer
	twine_cache_fetch_s3_(graph->uri, &tbuf, &l);
	if (!tbuf)
	{
		twine_logf(LOG_CRIT, "failed to load the triples\n");
		return -1;
	}

	// Populate the model
	graph->old = twine_rdf_model_create();
	if(!graph->old)
	{
		free(tbuf);
		twine_logf(LOG_CRIT, "failed to allocate an RDF model\n");
		return -1;
	}

	return 0;

	// TODO Update the index in the DB to be able to find the object back

	/** Index 1 : Find all the triples having <X> as a subject or object
	 * return all the triples and the name of the source they came from
	 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
	 * GRAPH ?g {
	 *  { <X> ?p ?o .
	 *  BIND(<X> as ?s)  }
	 *  UNION
	 *  { ?s ?p <X> .
	 *  BIND(<X> as ?o)  }
	 * }
	 * }
	**/
	// Table => Graph | {subject/object}
	// Query => Get the list of graphs, fetch them from S3, filter relevant triples

	/** Index 2 : For a given graph find all the media pointed at. Then
	 * fetch their descriptions too. The index actually concerns the first
	 * part of the query only
	 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
	 *  GRAPH <http://localhost/a9d3d9dac7804f4689789e455365b6c4> {
	 *   <http://localhost/a9d3d9dac7804f4689789e455365b6c4#id> ?p1 ?s .
	 *   FILTER(?p1 = <http://xmlns.com/foaf/0.1/page> ||
	 *   ?p1 = <http://search.yahoo.com/mrss/player> ||
	 *   ?p1 = <http://search.yahoo.com/mrss/content>)
	 *  }
	 *  GRAPH ?g {
	 *   ?s ?p ?o .
	 *  }
	 *  FILTER(?g != <http://localhost/a9d3d9dac7804f4689789e455365b6c4> &&
	 *  ?g != <http://localhost/>)
	 * }
	 */
	// Table => Graph | Subject | link_type | target_media
	// Query => Get the list of link_type+target_media, create triples,
	// fetch target descriptions and add them to the model

}

static int
twine_workflow_s3_put_(TWINE *restrict context, TWINEGRAPH *restrict graph, void *dummy)
{
	size_t l;
	char *tbuf;
	int r;

	(void) context;
	(void) dummy;

	twine_logf(LOG_DEBUG, "S3\n");

	// TODO Store the content of the ntriples as an object
	tbuf = twine_rdf_model_ntriples(graph->store, &l);
	if(tbuf)
	{
		r = twine_cache_store_s3_(graph->uri, tbuf, l);
		librdf_free_memory(tbuf);
	}
	else
	{
		r = -1;
	}
	return r;

	// TODO Update the index in the DB to be able to find the object back

	/** Index 1 : Find all the triples having <X> as a subject or object
	 * return all the triples and the name of the source they came from
	 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
	 * GRAPH ?g {
	 *  { <X> ?p ?o .
	 *  BIND(<X> as ?s)  }
	 *  UNION
	 *  { ?s ?p <X> .
	 *  BIND(<X> as ?o)  }
	 * }
	 * }
	**/
	// Table => Graph | {subject/object}
	// Query => Get the list of graphs, fetch them from S3, filter relevant triples

	/** Index 2 : For a given graph find all the media pointed at. Then
	 * fetch their descriptions too. The index actually concerns the first
	 * part of the query only
	 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
	 *  GRAPH <http://localhost/a9d3d9dac7804f4689789e455365b6c4> {
	 *   <http://localhost/a9d3d9dac7804f4689789e455365b6c4#id> ?p1 ?s .
	 *   FILTER(?p1 = <http://xmlns.com/foaf/0.1/page> ||
	 *   ?p1 = <http://search.yahoo.com/mrss/player> ||
	 *   ?p1 = <http://search.yahoo.com/mrss/content>)
	 *  }
	 *  GRAPH ?g {
	 *   ?s ?p ?o .
	 *  }
	 *  FILTER(?g != <http://localhost/a9d3d9dac7804f4689789e455365b6c4> &&
	 *  ?g != <http://localhost/>)
	 * }
	 */
	// Table => Graph | Subject | link_type | target_media
	// Query => Get the list of link_type+target_media, create triples,
	// fetch target descriptions and add them to the model

}

/* 's3-get' processor: get an RDF graph from the cache */
static int
twine_workflow_process_single_(TWINE *context, TWINEGRAPH *graph, const char *name)
{
	void *prev;
	size_t c;
	int r;

	twine_logf(LOG_DEBUG, "invoking graph processor '%s' for <%s>\n", name, graph->uri);
	prev = context->plugin_current;
	r = 0;
	for(c = 0; c < context->cbcount; c++)
	{
		if(context->callbacks[c].type == TCB_PROCESSOR &&
		   !strcmp(context->callbacks[c].m.processor.name, name))
		{
			context->plugin_current = context->callbacks[c].module;
			if(context->callbacks[c].m.processor.fn(context, graph, context->callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", context->callbacks[c].m.processor.name);
				r = -1;
			}
			break;
		}
		if(context->callbacks[c].type == TCB_LEGACY_GRAPH &&
		   !strcmp(context->callbacks[c].m.legacy_graph.name, name))
		{
			context->plugin_current = context->callbacks[c].module;
			if(context->callbacks[c].m.legacy_graph.fn(graph, context->callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", context->callbacks[c].m.legacy_graph.name);
				r = -1;
			}
			break;
		}
	}
	context->plugin_current = prev;
	return r;	
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
	TWINE *context;

	(void) key;

	context = (TWINE *) data;

	twine_logf(LOG_DEBUG, "adding processor '%s' to workflow\n", value);
	if(!twine_plugin_processor_exists(context, value))
	{
		twine_logf(LOG_CRIT, "graph processor '%s' named in workflow configuration does not exist (have all the necessary plug-ins been loaded?)\n", value);
		return -1;
	}
	p = (char **) realloc(workflow, sizeof(char *) * (nworkflow + 2));
	if(!p)
	{
		twine_logf(LOG_CRIT, "failed to expand workflow list buffer\n");
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

static int
twine_cache_store_s3_(const char *g, char *ntbuf, size_t bufsize)
{
	char *urlbuf;
	char nqlenstr[256];
	AWSREQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;
	char *t;
	AWSS3BUCKET *bucket;

	// Set the data to send
	s3data.buf = ntbuf;
	s3data.bufsize = bufsize;
	s3data.pos = 0;

	// Create a bucket "twine"
	bucket = aws_s3_create("twine");
	aws_s3_set_logger(bucket, twine_vlogf);
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		aws_s3_set_endpoint(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		aws_s3_set_access(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		aws_s3_set_secret(bucket, t);
		free(t);
	}

	// Prepare the request
	req = aws_s3_request_create(bucket, g, "PUT");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(ch, CURLOPT_READFUNCTION, twine_cache_store_s3_upload_);
	curl_easy_setopt(ch, CURLOPT_READDATA, &s3data);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE, (long) s3data.bufsize);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
	headers = curl_slist_append(aws_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Content-Type: " MIME_NQUADS);
	headers = curl_slist_append(headers, "x-amz-acl: public-read");
	sprintf(nqlenstr, "Content-Length: %u", (unsigned) s3data.bufsize);
	headers = curl_slist_append(headers, nqlenstr);
	aws_request_set_headers(req, headers);
	r = 0;
	twine_logf(LOG_DEBUG,"Request ok\n");
	if((e = aws_request_perform(req)))
	{
		twine_logf(LOG_ERR,": failed to upload N-Quads: %s\n", curl_easy_strerror(e));
		r = -1;
	}
	else
	{
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status != 200)
		{
			twine_logf(LOG_ERR, ": failed to upload N-Quads to bucket at <%s> (HTTP status %ld)\n", urlbuf, status);
			r = -1;
		}
	}
	aws_request_destroy(req);
	aws_s3_destroy(bucket);
	free(urlbuf);
	return r;
}

static int
twine_cache_fetch_s3_(const char *g, char **ntbuf, size_t *buflen)
{
	AWSREQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;
	char *t;
	AWSS3BUCKET *bucket;

	// Init the structures that will be returned
	*ntbuf = NULL;
	*buflen = 0;
	memset(&s3data, 0, sizeof(struct s3_upload_struct));

	// Create a bucket "twine"
	bucket = aws_s3_create("twine");
	aws_s3_set_logger(bucket, twine_vlogf);
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		aws_s3_set_endpoint(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		aws_s3_set_access(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		aws_s3_set_secret(bucket, t);
		free(t);
	}

	// Prepare the request
	req = aws_s3_request_create(bucket, g, "GET");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, twine_cache_store_s3_download_);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, &s3data);
	headers = curl_slist_append(aws_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Accept: " MIME_NQUADS);
	aws_request_set_headers(req, headers);
	r = 1;
	if((e = aws_request_perform(req)))
	{
		twine_logf(LOG_ERR, "failed to download buffer from bucket : %s\n", curl_easy_strerror(e));
		r = -1;
	}
	else
	{
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status == 404 || status == 403)
		{
			r = 0;
		}
		else if(status != 200)
		{
			twine_logf(LOG_ERR, "failed to download buffer from bucket : HTTP status %ld\n", status);
			r = -1;
		}
	}
	aws_request_destroy(req);
	if(r > 0)
	{
		twine_logf(LOG_DEBUG, "all fine!\n");
		*ntbuf = s3data.buf;
		*buflen = s3data.pos;
	}
	else
	{
		free(s3data.buf);
	}
	aws_s3_destroy(bucket);

	return r;
}

static size_t
twine_cache_store_s3_upload_(char *buffer, size_t size, size_t nitems, void *userdata)
{
	struct s3_upload_struct *data;

	data = (struct s3_upload_struct *) userdata;
	size *= nitems;
	if(size > data->bufsize - data->pos)
	{
		size = data->bufsize - data->pos;
	}
	memcpy(buffer, &(data->buf[data->pos]), size);
	data->pos += size;
	return size;
}

static size_t
twine_cache_store_s3_download_(char *buffer, size_t size, size_t nitems, void *userdata)
{
	struct s3_upload_struct *data;
	char *p;

	data = (struct s3_upload_struct *) userdata;
	size *= nitems;
	if(data->pos + size >= data->bufsize)
	{
		p = (char *) realloc(data->buf, data->bufsize + size + 1);
		if(!p)
		{
			twine_logf(LOG_CRIT, "failed to expand receive buffer\n");
			return 0;
		}
		data->buf = p;
		data->bufsize += size;
	}
	memcpy(&(data->buf[data->pos]), buffer, size);
	data->pos += size;
	data->buf[data->pos] = 0;
	twine_logf(LOG_DEBUG, "read %lu bytes\n", (unsigned long) size);
	return size;
}

