/* Twine: Internal API
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

/* Deprecated: return the path to the Twine configuration file */
const char *
twine_config_path(void)
{
	return DEFAULT_CONFIG_PATH;
}

/* Deprecated: return the default MQ URI */
const char *
twine_mq_default_uri(void)
{
	return DEFAULT_MQ_URI;
}

/* Deprecated: register an input handler
 * See twine_plugin_add_input()
 */
int
twine_plugin_register(const char *mimetype, const char *description, twine_processor_fn fn, void *data)
{
	struct twine_callback_struct *p;

	if(!twine_)
	{
		return -1;
	}
	p = twine_plugin_callback_add_(twine_, data);
	if(!p)
	{
		return -1;
	}
	p->m.legacy_mime.type = strdup(mimetype);
	p->m.legacy_mime.desc = strdup(description);
	if(!p->m.legacy_mime.type || !p->m.legacy_mime.desc)
	{
		free(p->m.legacy_mime.type);
		free(p->m.legacy_mime.desc);
		twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
		return -1;
	}
	p->m.legacy_mime.fn = fn;
	p->type = TCB_LEGACY_MIME;
	twine_logf(LOG_NOTICE, "Deprecated: registered legacy handler for MIME type: '%s' (%s)\n", mimetype, description);
	return 0;
}

/* Deprecated: register a bulk import handler
 * See twine_plugin_add_bulk()
 */
int
twine_bulk_register(const char *mimetype, const char *description, twine_bulk_fn fn, void *data)
{
	struct twine_callback_struct *p;

	if(!twine_)
	{
		return -1;
	}
	p = twine_plugin_callback_add_(twine_, data);
	if(!p)
	{
		return -1;
	}
	p->m.legacy_bulk.type = strdup(mimetype);
	p->m.legacy_bulk.desc = strdup(description);
	if(!p->m.legacy_bulk.type || !p->m.legacy_bulk.desc)
	{
		free(p->m.legacy_bulk.type);
		free(p->m.legacy_bulk.desc);
		twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
		return -1;
	}
	p->m.legacy_bulk.fn = fn;
	p->type = TCB_LEGACY_BULK;
	twine_logf(LOG_NOTICE, "Deprecated: registered legacy bulk-import handler for MIME type: '%s' (%s)\n", mimetype, description);
	return 0;
}

/* Deprecated: register a graph processor
 * See twine_plugin_add_processor()
 */
int
twine_graph_register(const char *name, twine_graph_fn fn, void *data)
{
	struct twine_callback_struct *g;

	if(!twine_)
	{
		return -1;
	}
	g = twine_plugin_callback_add_(twine_, data);
	if(!g)
	{
		return -1;
	}
	g->m.legacy_graph.name = strdup(name);
	if(!g->m.legacy_graph.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register graph processor\n");
		return -1;
	}
	g->m.legacy_graph.fn = fn;
	g->type = TCB_LEGACY_GRAPH;
	twine_logf(LOG_NOTICE, "Deprecated: registered legacy graph processor: '%s'\n", name);
	return 0;
}

/* Deprecated: register a post-processor
 * See twine_plugin_add_processor()
 */
int
twine_postproc_register(const char *name, twine_postproc_fn fn, void *data)
{
	struct twine_callback_struct *g;

	if(!twine_)
	{
		return -1;
	}
	g = twine_plugin_callback_add_(twine_, data);
	if(!g)
	{
		return -1;
	}
	g->m.legacy_graph.name = (char *) calloc(1, strlen(name) + 6);
	if(!g->m.legacy_graph.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register post-processor\n");
		return -1;
	}
	strcpy(g->m.legacy_graph.name, "post:");
	strcpy(&(g->m.legacy_graph.name[5]), name);
	g->m.legacy_graph.fn = fn;
	g->type = TCB_LEGACY_GRAPH;

	twine_logf(LOG_NOTICE, "Deprecated: registered legacy graph processor: 'post:%s'\n", name);
	return 0;
}

/* Deprecated: register a pre-processor
 * See twine_plugin_add_processor()
 */
int
twine_preproc_register(const char *name, twine_preproc_fn fn, void *data)
{
	struct twine_callback_struct *g;

	if(!twine_)
	{
		return -1;
	}
	g = twine_plugin_callback_add_(twine_, data);
	if(!g)
	{
		return -1;
	}
	g->m.legacy_graph.name = (char *) calloc(1, strlen(name) + 5);
	if(!g->m.legacy_graph.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register pre-processor\n");
		return -1;
	}
	strcpy(g->m.legacy_graph.name, "pre:");
	strcpy(&(g->m.legacy_graph.name[4]), name);
	g->m.legacy_graph.fn = fn;
	g->type = TCB_LEGACY_GRAPH;

	twine_logf(LOG_NOTICE, "Deprecated: registered legacy graph processor: 'pre:%s'\n", name);
	return 0;
}

/* Deprecated: register an update handler
 * See twine_plugin_add_update()
 */
int
twine_update_register(const char *name, twine_update_fn fn, void *data)
{
	struct twine_callback_struct *p;

	if(!twine_)
	{
		return -1;
	}
	p = twine_plugin_callback_add_(twine_, data);
	if(!p)
	{
		return -1;
	}
	p->m.legacy_update.name = strdup(name);
	if(!p->m.legacy_update.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register update handler\n");
		return -1;
	}
	p->m.legacy_update.fn = fn;
	p->type = TCB_LEGACY_UPDATE;
	twine_logf(LOG_NOTICE, "Deprecated: registered legacy update handler: '%s'\n", name);
	return 0;
}

/* Deprecated: Check whether a MIME type is supported by any input handler
 * See twine_plugin_input_exists()
 */
int
twine_plugin_supported(const char *mimetype)
{
	return twine_plugin_input_exists(twine_, mimetype);
}

/* Deprecated: Check whether a MIME type is supported by any bulk processor
 * See twine_plugin_bulk_exists()
 */
int
twine_bulk_supported(const char *mimetype)
{
	return twine_plugin_bulk_exists(twine_, mimetype);
}

/* Deprecated: Check whether a plug-in name is recognised as an update
 * handler
 * See twine_plugin_update_exists()
 */
int
twine_update_supported(const char *name)
{
	return twine_plugin_update_exists(twine_, name);
}

/* Deprecated: Check whether a plug-in name is recognised as an graph
 * processing handler
 * See twine_plugin_processor_exists()
 */
int
twine_graph_supported(const char *name)
{
	return twine_plugin_processor_exists(twine_, name);
}

/* Deprecated: process a single message of a given type
 * See twine_workflow_process_message()
 */
int
twine_plugin_process(const char *mimetype, const unsigned char *message, size_t msglen, const char *subject)
{
	return twine_workflow_process_message(twine_, mimetype, message, msglen, subject);
}

/* Deprecated: perform a bulk import from a file
 * See twine_workflow_process_file()
 */
int
twine_bulk_import(const char *mimetype, FILE *file)
{
	return twine_workflow_process_file(twine_, mimetype, file);
}

/* Deprecated: Ask a named plug-in to update the data about identifier
 * See twine_workflow_process_update()
 */
int
twine_update(const char *plugin, const char *identifier)
{
	return twine_workflow_process_update(twine_, plugin, identifier);
}

/* Deprecated: Replace a graph from a Turtle buffer
 * See twine_workflow_process_rdf()
 */
int
twine_sparql_put(const char *uri, const char *triples, size_t length)
{
	return twine_sparql_put_internal_(uri, triples, length, MIME_TURTLE, NULL);
}

/* Deprecated: Replace a graph with triples in a buffer in a specified
 * format
 * See twine_workflow_process_rdf()
 */
int
twine_sparql_put_format(const char *uri, const char *triples, size_t length, const char *type)
{
	return twine_sparql_put_internal_(uri, triples, length, type, NULL);
}

/* Deprecated: Replace a graph from a librdf stream 
 * See twine_workflow_process_rdf() and twine_workflow_process_graph()
 */
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

/* Deprecated: Replace a graph from a librdf model
 * See twine_workflow_process_graph()
 */
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

/* Private: Construct an RDF graph and pass it to the processing modules which
 * constitute the workflow.
 */
static int
twine_sparql_put_internal_(const char *uri, const char *triples, size_t length, const char *type, librdf_model *sourcemodel)
{
	int r;
	TWINEGRAPH *graph;
	librdf_stream *stream;

	if(sourcemodel)
	{
		graph = twine_graph_create(twine_, uri);
		stream = librdf_model_as_stream(sourcemodel);
		r = librdf_model_add_statements(graph->store, stream);
		librdf_free_stream(stream);
	}
	else
	{
		r = 0;
		graph = twine_graph_create_rdf(twine_, uri, (const unsigned char *) triples, length, type);
		if(!graph)
		{
			return -1;
		}
	}
	if(!r)
	{
		r = twine_workflow_process_graph(twine_, graph);
	}
	twine_graph_destroy(graph);
	return r;
}
