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

static SPINDLE spindle;

static int spindle_init_(SPINDLE *spindle);
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
	twine_postproc_register(PLUGIN_NAME, spindle_process, &spindle);
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
	spindle->root = twine_config_geta("spindle:graph", "http://localhost/");
	if(!spindle->root)
	{
		return -1;
	}
	spindle->sparql = twine_sparql_create();
	if(!spindle->sparql)
	{
		return -1;
	}
	spindle->sameas = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) "http://www.w3.org/2002/07/owl#sameAs");
	if(!spindle->sameas)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for owl:sameAs\n");
		return -1;
	}
	spindle->rdftype = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	if(!spindle->rdftype)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for rdf:type\n");
		return -1;
	}
	spindle->rootgraph = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) spindle->root);
	if(!spindle->rootgraph)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for <%s>\n", spindle->root);
		return -1;
	}	
	spindle->modified = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) "http://purl.org/dc/terms/modified");
	if(!spindle->modified)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for dct:modified\n");
		return -1;
	}
	spindle->xsd_dateTime = librdf_new_uri(spindle->world, (const unsigned char *) "http://www.w3.org/2001/XMLSchema#dateTime");
	if(!spindle->xsd_dateTime)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create URI for xsd:dateTime\n");
		return -1;
	}
	return 0;
}

static int
spindle_cleanup_(SPINDLE *spindle)
{
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
	return 0;
}
