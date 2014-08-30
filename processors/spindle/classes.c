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

/* This won't scale without inference at point of update */

struct classmatch_struct
{
	const char *uri;
	const char **match;
};

static const char *person_matches[] =  {
	"http://xmlns.com/foaf/0.1/Person",
	NULL
};

static const char *group_matches[] = {
	"http://xmlns.com/foaf/0.1/Group",
	"http://xmlns.com/foaf/0.1/Organization",
	NULL
};

static const char *agent_matches[] = {
	"http://xmlns.com/foaf/0.1/Agent",
	"http://purl.org/dc/terms/Agent",
	NULL,
};

static const char *location_matches[] = {
	"http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing",
	"http://purl.org/dc/terms/Location",
	"http://www.geonames.org/ontology#Feature",
	NULL,
};

static const char *physical_matches[] = {
	"http://www.cidoc-crm.org/cidoc-crm/E18_Physical_Thing",
	"http://www.cidoc-crm.org/cidoc-crm/E22_Man-Man_Object",
	"http://erlangen-crm.org/current/E18_Physical_Thing",
	"http://erlangen-crm.org/current/E22_Man-Made_Object",
	"http://purl.org/dc/dcmitype/PhysicalObject",
	"http://purl.org/dc/terms/PhysicalResource",
	NULL,
};

static const char *concept_matches[] = {
	"http://www.w3.org/2004/02/skos/core#Concept",
	"http://www.w3.org/2008/05/skos#Concept",
	NULL,
};

static const char *collection_matches[] = {
	"http://purl.org/dc/dcmitype/Collection",
	NULL
};

static const char *work_matches[] = {
	"http://purl.org/vocab/frbr/core#Work",
	NULL,
};

static const char *digital_matches[] = {
	"http://xmlns.com/foaf/0.1/Document",
	"http://xmlns.com/foaf/0.1/Image",
	NULL,
};

static struct classmatch_struct matches[] = {
	/* People */
	{
		"http://xmlns.com/foaf/0.1/Person",
		person_matches,
	},
	
	/* Organisations and groups */
	{
		"http://xmlns.com/foaf/0.1/Group",
		group_matches,
	},
	
	/* Any other kind of agent */
	{
		"http://xmlns.com/foaf/0.1/Agent",
		agent_matches,
	},
	
	/* Locations/spatial regions */
	{
		"http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing",
		location_matches,
	},
	
	/* Physical objects */
	{
		"http://www.cidoc-crm.org/cidoc-crm/E18_Physical_Thing",
		physical_matches,
	},

	/* Concepts */
	{
		"http://www.w3.org/2004/02/skos/core#Concept",
		concept_matches,
	},
	
	/* Collections */
	{
		"http://purl.org/dc/dcmitype/Collection",
		collection_matches,
	},

	/* Creative (or intellectual) works */
	{
		"http://purl.org/vocab/frbr/core#Work",
		work_matches,
	},
	
	/* Digital objects */
	{
		"http://xmlns.com/foaf/0.1/Document",		
		digital_matches,
	},
	{
		NULL,
		NULL
	}
};

/* Determine the class of something */
const char *
spindle_class_match(librdf_model *model, struct spindle_strset_struct *classes)
{
	librdf_statement *query, *st;
	librdf_node *node;
	librdf_stream *stream;
	librdf_uri *uri;
	unsigned char *uristr;
	size_t c, d;
	const char *match;

	match = NULL;
	node = librdf_new_node_from_uri_string(spindle_world, (const unsigned char *) "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	query = librdf_new_statement(spindle_world);
	librdf_statement_set_predicate(query, node);
	stream = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_object(st);
		if(librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = librdf_uri_as_string(uri)))
		{
			if(classes)
			{
				spindle_strset_add(classes, (const char *) uristr);
			}
			if(!match)
			{
				for(c = 0; matches[c].uri; c++)
				{
					for(d = 0; matches[c].match[d]; d++)
					{
						if(!strcmp((const char *) uristr, matches[c].match[d]))
						{
							match = matches[c].uri;							
							if(classes)
							{
								spindle_strset_add(classes, match);
							}
							break;
						}
					}
					if(match)
					{
						break;
					}
				}
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(!match)
	{
		twine_logf(LOG_WARNING, PLUGIN_NAME ": no class match for object\n");
	}
	return match;
}

/* Update the classes of a proxy */
const char *
spindle_class_update(const char *localname, librdf_model *model)
{
	const char *classname;
	struct spindle_strset_struct *classes;
	size_t c, len;
	char *buf, *sp;

	classes = spindle_strset_create();
	if(!classes)
	{
		return NULL;
	}
	classname = spindle_class_match(model, classes);
	len = strlen(localname) + strlen(spindle_root) + 100;
	for(c = 0; c < classes->count; c++)
	{
		len += strlen(classes->strings[c]) + 5;
	}
	buf = (char *) calloc(1, len);
	if(!buf)
	{
		twine_logf(LOG_CRIT, "failed to allocate SPARQL update buffer\n");
		spindle_strset_destroy(classes);
		return NULL;
	}
	sp = buf;
	sp += sprintf(sp, "PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#>\n"
				  "INSERT DATA {\n"
				  "GRAPH <%s> {\n"
				  "<%s> rdf:type ", spindle_root, localname);
	for(c = 0; c < classes->count; c++)
	{
		sp += sprintf(sp, "<%s> %c ", classes->strings[c], (c + 1 == classes->count ? '.' : ','));
	}
	strcpy(sp, "} }");
	spindle_strset_destroy(classes);
	if(sparql_update(spindle_sparql, buf, strlen(buf)))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to perform SPARQL update\n");
		free(buf);
		return NULL;
	}
	free(buf);
	
	return classname;
}
