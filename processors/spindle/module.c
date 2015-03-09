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

static SPINDLE spindle;

static int spindle_init_(SPINDLE *spindle);
static int spindle_s3_init_(SPINDLE *spindle);
static int spindle_cleanup_(SPINDLE *spindle);

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{	
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	if(spindle_init_(&spindle))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": initialisation failed\n");
		spindle_cleanup_(&spindle);
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": URI prefix is <%s>\n", spindle.root);
	twine_preproc_register(PLUGIN_NAME, spindle_preproc, &spindle);
	twine_postproc_register(PLUGIN_NAME, spindle_postproc, &spindle);
	return 0;
}

int
twine_plugin_done(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: cleaning up\n");
	spindle_cleanup_(&spindle);
	return 0;
}

static int
spindle_init_(SPINDLE *spindle)
{
	memset(spindle, 0, sizeof(SPINDLE));
	spindle->world = twine_rdf_world();
	if(!spindle->world)
	{
		return -1;
	}
	spindle->multigraph = twine_config_get_bool("spindle:multigraph", 0);
	spindle->root = twine_config_geta("spindle:graph", NULL);
	if(!spindle->root)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to obtain Spindle root graph name\n");
		return -1;
	}
	spindle->sparql = twine_sparql_create();
	if(!spindle->sparql)
	{
		return -1;
	}
	spindle->sameas = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) NS_OWL "sameAs");
	if(!spindle->sameas)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for owl:sameAs\n");
		return -1;
	}
	spindle->rdftype = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) NS_RDF "type");
	if(!spindle->rdftype)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for rdf:type\n");
		return -1;
	}
	spindle->rootgraph = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) spindle->root);
	if(!spindle->rootgraph)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for <%s>\n", spindle->root);
		return -1;
	}	
	spindle->modified = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) NS_DCTERMS "modified");
	if(!spindle->modified)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for dct:modified\n");
		return -1;
	}
	spindle->xsd_dateTime = librdf_new_uri(spindle->world, (const unsigned char *) NS_XSD "dateTime");
	if(!spindle->xsd_dateTime)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create URI for xsd:dateTime\n");
		return -1;
	}
	spindle->graphcache = (struct spindle_graphcache_struct *) calloc(SPINDLE_GRAPHCACHE_SIZE, sizeof(struct spindle_graphcache_struct));
	if(!spindle->graphcache)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create graph cache\n");
		return -1;
	}
	if(spindle_rulebase_init(spindle))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to load rulebase\n");
		return -1;
	}
	if(spindle_s3_init_(spindle))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to initialise S3 bucket\n");
		return -1;
	}
	if(spindle_doc_init(spindle))
	{
		return -1;
	}
	if(spindle_license_init(spindle))
	{
		return -1;
	}
	return 0;
}

static int
spindle_s3_init_(SPINDLE *spindle)
{
	char *t;

	t = twine_config_geta("spindle:bucket", NULL);
	if(!t)
	{
		return 0;
	}
	spindle->bucket = s3_create(t);
	if(!spindle->bucket)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create S3 bucket object for <s3://%s>\n", t);
		free(t);		
		return -1;
	}
	s3_set_logger(spindle->bucket, twine_vlogf);
	free(t);
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		s3_set_endpoint(spindle->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		s3_set_access(spindle->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		s3_set_secret(spindle->bucket, t);
		free(t);
	}
	spindle->s3_verbose = twine_config_get_bool("s3:verbose", 0);
	return 0;
}

static int
spindle_cleanup_(SPINDLE *spindle)
{
	size_t c;

	if(spindle->sparql)
	{
		sparql_destroy(spindle->sparql);
	}
	if(spindle->root)
	{
		free(spindle->root);
	}
	if(spindle->sameas)
	{
		librdf_free_node(spindle->sameas);
	}
	if(spindle->rdftype)
	{
		librdf_free_node(spindle->rdftype);
	}
	if(spindle->rootgraph)
	{
		librdf_free_node(spindle->rootgraph);
	}
	if(spindle->modified)
	{
		librdf_free_node(spindle->modified);
	}
	if(spindle->xsd_dateTime)
	{
		librdf_free_uri(spindle->xsd_dateTime);
	}
	for(c = 0; spindle->graphcache && c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			continue;
		}
		twine_rdf_model_destroy(spindle->graphcache[c].model);
		free(spindle->graphcache[c].uri);
	}
	free(spindle->graphcache);
	free(spindle->titlepred);
	spindle_rulebase_cleanup(spindle);
	return 0;
}

/* Discard cached information about a graph */
int
spindle_graph_discard(SPINDLE *spindle, const char *uri)
{
	size_t c;

	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			continue;
		}
		if(!strcmp(spindle->graphcache[c].uri, uri))
		{
			twine_rdf_model_destroy(spindle->graphcache[c].model);
			free(spindle->graphcache[c].uri);
			memset(&(spindle->graphcache[c]), 0, sizeof(struct spindle_graphcache_struct));
			return 0;
		}
	}
	return 0;
}

/* Fetch information about a graph */
int
spindle_graph_description_node(SPINDLE *spindle, librdf_model *target, librdf_node *graph)
{
	size_t c;
	librdf_stream *stream;
	librdf_uri *uri;
	const char *uristr;

	uri = librdf_node_get_uri(graph);
	uristr = (const char *) librdf_uri_as_string(uri);
	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			continue;
		}
		if(!strcmp(spindle->graphcache[c].uri, uristr))
		{
			stream = librdf_model_context_as_stream(spindle->graphcache[c].model, graph);
			twine_rdf_model_add_stream(target, stream, graph);
			librdf_free_stream(stream);
			return 0;
		}
	}
	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			break;
		}
	}
	if(c == SPINDLE_GRAPHCACHE_SIZE)
	{
		twine_rdf_model_destroy(spindle->graphcache[0].model);
		free(spindle->graphcache[0].uri);
		memmove(&(spindle->graphcache[0]), &(spindle->graphcache[1]),
				sizeof(struct spindle_graphcache_struct) * (SPINDLE_GRAPHCACHE_SIZE - 1));
		c = SPINDLE_GRAPHCACHE_SIZE - 1;
		spindle->graphcache[c].model = NULL;
		spindle->graphcache[c].uri = NULL;
	}
	spindle->graphcache[c].model = twine_rdf_model_create();
	spindle->graphcache[c].uri = strdup(uristr);
	if(sparql_queryf_model(spindle->sparql, spindle->graphcache[c].model,
						   "SELECT DISTINCT ?s ?p ?o ?g\n"
						   " WHERE {\n"
						   "  GRAPH ?g {\n"
						   "   ?s ?p ?o .\n"
						   "   FILTER (?g = %V && ?s = ?g)\n"
						   "  }\n"
						   "}",
						   graph))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to fetch a graph description\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": added graph <%s> to cache\n", uristr);
	stream = librdf_model_context_as_stream(spindle->graphcache[c].model, graph);
	librdf_model_context_add_statements(target, graph, stream);
	librdf_free_stream(stream);
	return 0;
}
