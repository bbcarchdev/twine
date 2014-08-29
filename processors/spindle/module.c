/* Spindle: Co-reference aggregation engine
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

/* Spindle is a co-reference aggregation engine which works as a
 * post-processing hook for Twine. When a graph is updated by a Twine
 * processor, Spindle examines the contents of the graph and looks for
 * owl:sameAs co-reference assertions. Where some are found, it generates
 * "proxy" entities within a named graph which connect together all of
 * the coreferences for a given source entity, and as more co-references are
 * discovered, they're also added to the proxy entity.
 *
 * For example, if graphs are processed which assert that <A> owl:sameAs <B>,
 * <B> owl:sameAs <C> and <C> owl:sameAs <D>, the end result is the following:
 *
 * <http://spindle.example.com/> {
 *
 *   <http://spindle.example.com/abc123#id>
 *     owl:sameAs <A>, <B>, <C>, <D> .
 *
 * }
 *
 * By collecting together the co-references in this fashion, it becomes
 * possible to deal with a unified view (stored as quads internally) of
 * a given entity described by multiple disparate graphs.
 *
 * In other words, Spindle constructs a topic-oriented index of all of the
 * entities processed by Twine.
 */

/* TODO:
 *   Replace SPARQL query generation logic once supporting functionality
 *   has been added to libsparqlclient
 *
 *   Extraction of browse metadata for attaching to proxies
 *
 *   Class derivation should be described in RDF
 *
 *   Sifting/splitting on de-assertion
 *
 *   Same-origin policies
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_spindle.h"

librdf_world *spindle_world;
char *spindle_root;
SPARQL *spindle_sparql;

static int spindle_process_(librdf_model *newgraph, librdf_model *oldgraph, const char *uri);

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	twine_postproc_register(PLUGIN_NAME, spindle_process_);
	spindle_world = twine_rdf_world();
	if(!spindle_world)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain librdf world\n");
		return -1;
	}
	spindle_root = twine_config_geta("spindle.graph", "http://localhost/");
	if(!spindle_root)
	{
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": local graph prefix is <%s>\n", spindle_root);
	spindle_sparql = twine_sparql_create();
	if(!spindle_sparql)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create SPARQL connection\n");
		free(spindle_root);
		spindle_root = NULL;
		return -1;
	}
	return 0;
}

/* Post-processing hook, invoked by Twine operations */
static int
spindle_process_(librdf_model *newgraph, librdf_model *oldgraph, const char *graph)
{
	struct spindle_corefset_struct *oldset, *newset;
	struct spindle_strset_struct *changes;
	size_t c;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": processing updated graph <%s>\n", graph);
	changes = spindle_strset_create();
	if(!changes)
	{
		return -1;
	}
	/* find all owl:sameAs refs where either side is same-origin as graph */
	oldset = spindle_coref_extract(oldgraph, graph);
	if(!oldset)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to extract co-references from previous graph state\n");
		return -1;
	}
	newset = spindle_coref_extract(newgraph, graph);
	if(!newset)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to extract co-references from new graph state\n");
		return -1;
	}
	/* For each co-reference in the new graph, represent that within our
	 * local proxy graph
	 */
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": new graph contains %d coreferences\n", (int) newset->refcount);
	for(c = 0; c < newset->refcount; c++)
	{
		if(spindle_proxy_create(newset->refs[c].left, newset->refs[c].right, changes))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create proxy entity\n");
			spindle_coref_destroy(oldset);
			spindle_coref_destroy(newset);
			spindle_strset_destroy(changes);
			return -1;
		}
	}
	/* if oldgraph was provided, find all owl:sameAs refs in oldgraph which
	 * don't appear in newgraph
	 */
	/* for each, invoke remove_coref() */
	spindle_coref_destroy(oldset);
	spindle_coref_destroy(newset);
	/* Re-build the metadata for any related proxies */
	spindle_cache_update_set(changes);
	spindle_strset_destroy(changes);
	return 0;
}

