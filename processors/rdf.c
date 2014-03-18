/* Twine: RDF (quad) processing
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
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

static int process_rdf(const char *mime, const char *buf, size_t buflen);

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{
	twine_logf(LOG_DEBUG, "rdf plug-in: initialising\n");
	twine_plugin_register("application/trig", "RDF TriG", process_rdf);
	twine_plugin_register("application/nquads", "RDF N-Quads", process_rdf);
	return 0;
}

/* Process some kind of RDF and import the named graphs into the store
 *
 * Although in principle this processor can handle anything that librdf can
 * parse, it will do nothing unless there are named graphs present, and so
 * only N-Quads and TriG are listed in ../mimetypes.h
 */

static int
process_rdf(const char *mime, const char *buf, size_t buflen)
{
	librdf_model *model;
	librdf_iterator *iter;
	librdf_node *node;
	librdf_uri *uri;
	librdf_stream *stream;

	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_ERR, "failed to create new RDF model\n");
		return -1;
	}

	if(twine_rdf_model_parse(model, mime, buf, buflen))
	{
		twine_logf(LOG_ERR, "failed to parse string into model\n");
		return -1;
	}
	iter = librdf_model_get_contexts(model);
	if(!iter)
	{
		twine_logf(LOG_ERR, "failed to retrieve contexts from model\n");
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
			twine_sparql_put_stream((const char *) librdf_uri_as_string(uri), stream);
			/* XXX error-checking */
			librdf_free_stream(stream);
		}
		librdf_iterator_next(iter);
	}

	librdf_free_iterator(iter);
	librdf_free_model(model);

	return 0;
}

