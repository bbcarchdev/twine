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

#include "libtwine.h"

#define TWINE_PLUGIN_NAME               "rdf"

static int process_rdf(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, const char *restrict subject, void *data);

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
	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_ERR, "failed to create new RDF model\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, "parsing buffer into model as '%s'\n", mime);
	if(twine_rdf_model_parse(model, mime, (const char *) buf, buflen))
	{
		twine_logf(LOG_ERR, "failed to parse string into model\n");
		twine_rdf_model_destroy(model);
		return -1;
	}
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

