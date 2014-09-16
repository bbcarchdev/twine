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

static int spindle_cache_init_(SPINDLECACHE *data, SPINDLE *spindle, const char *localname);
static int spindle_cache_cleanup_(SPINDLECACHE *data);

/* Re-build the cached data for a set of proxies */
int
spindle_cache_update_set(SPINDLE *spindle, struct spindle_strset_struct *set)
{ 
	size_t c, origcount;

	/* Keep track of how many things were in the original set, so that we
	 * don't recursively re-cache a huge amount
	 */
	origcount = set->count;
	for(c = 0; c < set->count; c++)
	{
		if(c >= origcount)
		{
			spindle_cache_update(spindle, set->strings[c], NULL);
		}
		else
		{
			spindle_cache_update(spindle, set->strings[c], set);
		}
	}
	return 0;
}

/* Re-build the cached data for the proxy entity identified by localname;
 * if no references exist any more, the cached data will be removed.
 */
int
spindle_cache_update(SPINDLE *spindle, const char *localname, struct spindle_strset_struct *set)
{
	SPINDLECACHE data;
	int r;
	size_t c;
	struct spindle_strset_struct *subjects;
	librdf_statement *query, *st;
	librdf_node *node;
	librdf_uri *uri;	
	librdf_stream *stream, *qstream;
	const char *uristr;
	SPARQLRES *res;
	SPARQLROW *row;

	if(spindle_cache_init_(&data, spindle, localname))
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": updating <%s>\n", localname);
	/* Find all of the triples related to all of the subjects linked to the
	 * proxy.
	 */
	r = sparql_queryf_model(spindle->sparql, data.sourcedata,
							"SELECT DISTINCT ?s ?p ?o ?g\n"
							" WHERE {\n"
							"  GRAPH %V {\n"
							"   ?s %V %V .\n"
							"  }\n"
							"  GRAPH ?g {\n"
							"   ?s ?p ?o .\n"
							"  }\n"
							"}",
							spindle->rootgraph, data.sameas, data.self);

	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain cached data from SPARQL store\n");
		spindle_cache_cleanup_(&data);
		return -1;
	}   

	/* Copy any of our own owl:sameAs references into the proxy graph */
	query = twine_rdf_st_create();
	if(!query)
	{
		return -1;
		spindle_cache_cleanup_(&data);
	}
	node = twine_rdf_node_clone(data.sameas);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		spindle_cache_cleanup_(&data);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	node = twine_rdf_node_clone(data.self);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		spindle_cache_cleanup_(&data);
		return -1;
	}
	librdf_statement_set_object(query, node);
	/* Create a stream querying for (?s owl:sameAs <self>) in the root graph */
	stream = librdf_model_find_statements_with_options(data.sourcedata, query, spindle->rootgraph, NULL);
	if(!stream)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query model\n");
		twine_rdf_st_destroy(query);
		spindle_cache_cleanup_(&data);
		return -1;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		/* Check if the owl:sameAs statement is already present in the proxy
		 * data graph
		 */
		qstream = librdf_model_find_statements_with_options(data.proxydata, st, data.graph, NULL);
		if(!qstream)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query model\n");
			librdf_free_stream(stream);
			twine_rdf_st_destroy(query);
			spindle_cache_cleanup_(&data);
			return -1;
		}
		if(!librdf_stream_end(qstream))
		{
			/* If so, skip to the next item */
			librdf_free_stream(qstream);
			librdf_stream_next(stream);
			continue;
		}
		librdf_free_stream(qstream);
		/* Add the owl:sameAs statement to the proxy data graph */
		if(librdf_model_context_add_statement(data.proxydata, data.graph, st))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to proxy model\n");
			librdf_free_stream(stream);
			twine_rdf_st_destroy(query);
			spindle_cache_cleanup_(&data);
			return -1;
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);

	/* Remove our own derived proxy data from the source model */
	librdf_model_context_remove_statements(data.sourcedata, data.graph);
	if(data.graph != spindle->rootgraph)
	{
		librdf_model_context_remove_statements(data.sourcedata, spindle->rootgraph);
	}
	/* For anything which is the subject of one of the triples in the source
	 * dataset, find any triples whose object is that thing and add the
	 * subjects to the set if they're not already present.
	 */
	if(set)
	{
		subjects = spindle_strset_create();
		stream = librdf_model_as_stream(data.sourcedata);
		while(!librdf_stream_end(stream))
		{
			st = librdf_stream_get_object(stream);
			node = librdf_statement_get_subject(st);
			if(librdf_node_is_resource(node) &&
			   (uri = librdf_node_get_uri(node)) &&
			   (uristr = (const char *) librdf_uri_as_string(uri)))
			{
				spindle_strset_add(subjects, uristr);
			}
			librdf_stream_next(stream);
		}
		librdf_free_stream(stream);
		
		for(c = 0; c < subjects->count; c++)
		{
			res = sparql_queryf(spindle->sparql,
								"SELECT ?local, ?s WHERE {\n"
								" GRAPH %V {\n"
								"  ?s <http://www.w3.org/2002/07/owl#sameAs> ?local .\n"
								" }\n"
								" GRAPH ?g {\n"
								"   ?s ?p <%s> .\n"
								" }\n"
								"}",
								spindle->rootgraph, subjects->strings[c]);
			if(!res)
			{
				twine_logf(LOG_ERR, "SPARQL query for inbound references failed\n");
				spindle_strset_destroy(subjects);
				spindle_cache_cleanup_(&data);
				return -1;
			}
			while((row = sparqlres_next(res)))
			{
				node = sparqlrow_binding(row, 0);
				if(node && librdf_node_is_resource(node) &&
				   (uri = librdf_node_get_uri(node)) &&
				   (uristr = (const char *) librdf_uri_as_string(uri)))
				{
					spindle_strset_add(set, uristr);
				}				
			}
			sparqlres_destroy(res);
		}
		spindle_strset_destroy(subjects);
	}
	/* Add the cache triples to the new proxy model */
	if(spindle_class_update(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	if(spindle_prop_update(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}	
	/* Delete the old cache triples.
	 * Note that our owl:sameAs statements take the form
	 * <external> owl:sameAs <proxy>, so we can delete <proxy> ?p ?o with
	 * impunity.
	 */	
	r = sparql_updatef(spindle->sparql,
					   "WITH %V\n"
					   " DELETE { %V ?p ?o }\n"
					   " WHERE { %V ?p ?o }",
					   data.graph, data.self, data.self);   
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Insert the new proxy triples, if any */
	r = sparql_insert_model(spindle->sparql, data.proxydata);

	spindle_cache_cleanup_(&data);
	return r;
}

static int
spindle_cache_init_(SPINDLECACHE *data, SPINDLE *spindle, const char *localname)
{	
	const char *t;

	memset(data, 0, sizeof(SPINDLECACHE));
	data->spindle = spindle;
	data->sparql = spindle->sparql;
	data->localname = localname;
	data->self = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) localname);
	if(!data->self)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for <%s>\n", localname);
		return -1;
	}
	if(spindle->multigraph)
	{
		t = strchr(localname, '#');
		if(!t)
		{
			t = strchr(localname, 0);
		}
		data->graph = librdf_new_node_from_counted_uri_string(spindle->world, (unsigned const char *) localname, t - localname);
	}
	else
	{
		data->graph = spindle->rootgraph;
	}
	data->sameas = spindle->sameas;
	if(!(data->sourcedata = twine_rdf_model_create()))
	{
		return -1;
	}
	if(!(data->proxydata = twine_rdf_model_create()))
	{
		return -1;
	}
	return 0;
}

static int
spindle_cache_cleanup_(SPINDLECACHE *data)
{
	if(data->proxydata)
	{
		librdf_free_model(data->proxydata);
	}
	if(data->sourcedata)
	{
		librdf_free_model(data->sourcedata);
	}
	if(data->graph && data->graph != data->spindle->rootgraph)
	{
		librdf_free_node(data->graph);
	}
	if(data->self)
	{
		librdf_free_node(data->self);
	}
	return 0;
}
