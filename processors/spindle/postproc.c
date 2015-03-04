/* Spindle: Co-reference aggregation engine
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

#include "p_spindle.h"

/* Post-processing hook, invoked by Twine operations */
int
spindle_postproc(twine_graph *graph, void *data)
{
	SPINDLE *spindle;
	struct spindle_corefset_struct *oldset, *newset;
	struct spindle_strset_struct *changes;
	size_t c;

	spindle = (SPINDLE *) data;
	twine_logf(LOG_INFO, PLUGIN_NAME ": evaluating updated graph <%s>\n", graph->uri);
	spindle_graph_discard(spindle, graph->uri);
	changes = spindle_strset_create();
	if(!changes)
	{
		return -1;
	}
	/* find all owl:sameAs refs where either side is same-origin as graph */
	oldset = spindle_coref_extract(spindle, graph->old, graph->uri);
	if(!oldset)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to extract co-references from previous graph state\n");
		return -1;
	}
	newset = spindle_coref_extract(spindle, graph->store ? graph->store : graph->pristine, graph->uri);
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
		if(spindle_proxy_create(spindle, newset->refs[c].left, newset->refs[c].right, changes))
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
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": updating caches for <%s>\n", graph->uri);
	spindle_cache_update_set(spindle, changes);
	spindle_strset_destroy(changes);
	twine_logf(LOG_INFO, PLUGIN_NAME ": processing complete for graph <%s>\n", graph->uri);
	return 0;
}

