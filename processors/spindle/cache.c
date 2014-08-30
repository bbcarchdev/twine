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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_spindle.h"

/* Re-build the cached data for a set of proxies */
int
spindle_cache_update_set(struct spindle_strset_struct *set)
{
	size_t c;

	for(c = 0; c < set->count; c++)
	{
		spindle_cache_update(set->strings[c]);
	}
	return 0;
}

/* Re-build the cached data for the proxy entity identified by localname;
 * if no references exist any more, the cached data will be removed.
 */
int
spindle_cache_update(const char *localname)
{
	const char *classname;
	char *buf;
	size_t l;
	int r;
	librdf_model *model;
	librdf_node *ctxnode;
	
	l = strlen(spindle_root) + (strlen(localname) * 2) + 127;
	buf = (char *) calloc(1, l + 1);
	/* Note that our owl:sameAs statements take the form
	 * <remote> owl:sameAs <local>, so we can delete <local> ?p ?o with
	 * impunity.
	 */
	snprintf(buf, l, "WITH <%s>\n"
			 "DELETE { <%s> ?p ?o }\n"
			 "WHERE { <%s> ?p ?o }\n",
			 spindle_root, localname, localname);
	if(sparql_update(spindle_sparql, buf, strlen(buf)))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
		free(buf);
		return -1;
	}
	snprintf(buf, l, "SELECT ?s ?p ?o ?g\n"
			 "WHERE {\n"
			 "GRAPH <%s> {\n"
			 "?s <http://www.w3.org/2002/07/owl#sameAs> <%s> .\n"
			 "}\n"
			 "GRAPH ?g {\n"
			 "?s ?p ?o .\n"
			 "}\n"
			 "}", spindle_root, localname);
	model = twine_rdf_model_create();
	r = sparql_query_model(spindle_sparql, buf, strlen(buf), model);
	free(buf);
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain cached data from SPARQL store\n");
		librdf_free_model(model);
		return -1;
	}   
	ctxnode = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) spindle_root);
	if(!ctxnode)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create URI node\n");
		librdf_free_model(model);
		return -1;
	}		
	librdf_model_context_remove_statements(model, ctxnode);
	librdf_free_node(ctxnode);
	
	classname = spindle_class_update(localname, model);
	spindle_predicate_update(localname, model, classname);
	
	librdf_free_model(model);

	return 0;
}
