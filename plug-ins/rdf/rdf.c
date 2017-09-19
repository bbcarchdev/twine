/* Twine: RDF (quad) processing
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

#include "libtwine.h"

#define TWINE_PLUGIN_NAME               "rdf"

static int process_rdf(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, const char *restrict subject, void *data);
static int dump_nquads(TWINE *restrict context, TWINEGRAPH *restrict graph, void *data);

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
	size_t graphtotal, graphcount;
	int r;
	CLUSTERJOB *job;

	(void) subject;
	(void) data;

	r = 0;
	job = twine_job(context);
	model = twine_rdf_model_create();
	if(!model)
	{
		cluster_job_logf(job, LOG_CRIT, TWINE_PLUGIN_NAME ": failed to create new RDF model\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": parsing buffer into model as '%s'\n", mime);
	if(twine_rdf_model_parse(model, mime, (const char *) buf, buflen))
	{
		cluster_job_logf(job, LOG_ERR, TWINE_PLUGIN_NAME ": failed to parse %s buffer of %lu bytes into model\n", mime, (unsigned long) buflen);
		twine_rdf_model_destroy(model);
		return -1;
	}
	iter = librdf_model_get_contexts(model);
	if(!iter)
	{
		cluster_job_logf(job, LOG_ERR, TWINE_PLUGIN_NAME ": failed to obtain named graphs iterator\n");
		twine_rdf_model_destroy(model);
		return -1;
	}
	if(librdf_iterator_end(iter))
	{
		cluster_job_logf(job, LOG_ERR, TWINE_PLUGIN_NAME ": parsed model contains no named graphs to process\n");
		librdf_free_iterator(iter);
		twine_rdf_model_destroy(model);
		cluster_job_set_total(job, 0);
		return -1;
	}
	for(graphtotal = 0; !librdf_iterator_end(iter); graphtotal++)
	{
		librdf_iterator_next(iter);
	}
	cluster_job_set_total(job, graphtotal);
	librdf_free_iterator(iter);
	graphcount = 0;
	iter = librdf_model_get_contexts(model);
	if(!iter)
	{
		cluster_job_logf(job, LOG_ERR, TWINE_PLUGIN_NAME ": failed to obtain named graphs iterator\n");
		twine_rdf_model_destroy(model);
		return -1;
	}
	while(!librdf_iterator_end(iter))
	{
		cluster_job_set_progress(job, graphcount);
		node = (librdf_node *) librdf_iterator_get_object(iter);
		if(!node)
		{
			graphcount++;
			continue;
		}
		else if(librdf_node_is_resource(node))
		{
			uri = librdf_node_get_uri(node);
			twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": processing graph %d of %d: <%s>\n", graphcount + 1, graphtotal, (const char *) librdf_uri_as_string(uri));
			stream = librdf_model_context_as_stream(model, node);
			if(twine_workflow_process_stream(context, (const char *) librdf_uri_as_string(uri), stream))
			{
				cluster_job_logf(job, LOG_ERR, TWINE_PLUGIN_NAME ": failed to process graph <%s>\n", (const char *) librdf_uri_as_string(uri));
				r = 1;
				break;
			}
			librdf_free_stream(stream);
		}
		librdf_iterator_next(iter);
		graphcount++;
	}
	cluster_job_set_progress(job, graphcount);
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
