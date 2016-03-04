/* Twine: Graph object handling
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

/* Public: create a new empty graph object with the supplied URI */
TWINEGRAPH *
twine_graph_create(TWINE *restrict context, const char *restrict uri)
{
	TWINEGRAPH *p;

	(void) context;

	p = (TWINEGRAPH *) calloc(1, sizeof(TWINEGRAPH));
	if(!p)
	{
		return NULL;
	}
	p->uri = strdup(uri);
	p->store = twine_rdf_model_create();
	if(!p->uri || !p->store)
	{
		twine_graph_destroy(p);
		return NULL;
	}
	return p;
}

/* Public: create a new graph object with the supplied URI by parsing a
 * buffer containing triples in a supported format (specified by type)
 */
TWINEGRAPH *
twine_graph_create_rdf(TWINE *restrict context, const char *restrict uri, const unsigned char *restrict buf, size_t buflen, const char *restrict type)
{
	TWINEGRAPH *p;

	p = twine_graph_create(context, uri);
	if(!p)
	{
		return NULL;
	}
	if(twine_rdf_model_parse(p->store, type, (const char *) buf, buflen))
	{
		twine_graph_destroy(p);
		return NULL;
	}
	return p;
}

/* Public: release the resources associated with a graph object */
int
twine_graph_destroy(TWINEGRAPH *graph)
{
	if(graph->old)
	{
		twine_rdf_model_destroy(graph->old);
	}
	if(graph->store)
	{
		twine_rdf_model_destroy(graph->store);
	}
	free(graph->uri);
	return 0;
}

/* Public: return the URI associated with a graph object */
const char *
twine_graph_uri(TWINEGRAPH *graph)
{
	return graph->uri;
}

/* Public: return the librdf_model associated with a graph object */
librdf_model *
twine_graph_model(TWINEGRAPH *graph)
{
	return graph->store;
}

/* Public: return the librdf_model that contains original data associated with
 * a graph object
 */
librdf_model *
twine_graph_orig_model(TWINEGRAPH *graph)
{
	return graph->old;
}
