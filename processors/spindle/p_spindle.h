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

#ifndef P_SPINDLE_H_
# define P_SPINDLE_H_                   1

# define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <uuid/uuid.h>

#include "libtwine.h"

/* The name of this plug-in */
#define PLUGIN_NAME                     "spindle"

/* The number of co-references allocated at a time when extending a set */
#define SET_BLOCKSIZE                   4

typedef struct spindle_context_struct SPINDLE;
typedef struct spindle_cache_struct SPINDLECACHE;

struct spindle_context_struct
{
	/* The librdf execution context from Twine */
	librdf_world *world;
	/* The URI of our root graph, and prefix for proxy entities */
	char *root;
	/* The SPARQL connection handle from Twine */
	SPARQL *sparql;
	/* rdf:type */
	librdf_node *rdftype;
	/* owl:sameAs */
	librdf_node *sameas;
	/* dct:modified */
	librdf_node *modified;
	/* xsd:dateTime */
	librdf_uri *xsd_dateTime;
	/* The root URI as a librdf_node */
	librdf_node *rootgraph;
	/* Whether to store each proxy in its own graph */
	int multigraph;
};

struct spindle_coref_struct
{
	char *left;
	char *right;
};

struct spindle_corefset_struct
{
	struct spindle_coref_struct *refs;
	size_t refcount;
	size_t size;
};

struct spindle_strset_struct
{
	char **strings;
	size_t count;
	size_t size;
};

struct spindle_cache_struct
{
	SPINDLE *spindle;
	SPARQL *sparql;
	const char *localname;
	const char *classname;
	librdf_model *sourcedata;
	librdf_model *proxydata;
	librdf_node *graph;
	librdf_node *self;
	librdf_node *sameas;
};

/* Post-process an updated graph */
int spindle_process(librdf_model *newgraph, librdf_model *oldgraph, const char *graph, void *data);

/* Extract a list of co-references from a librdf model */
struct spindle_corefset_struct *spindle_coref_extract(SPINDLE *spindle, librdf_model *model, const char *graphuri);
/* Add a single co-reference to a set */
int spindle_coref_add(struct spindle_corefset_struct *set, const char *l, const char *r);
/* Free the resources used by a co-reference set */
int spindle_coref_destroy(struct spindle_corefset_struct *set);

/* Create an empty string-set */
struct spindle_strset_struct *spindle_strset_create(void);
/* Add a string to a string-set */
int spindle_strset_add(struct spindle_strset_struct *set, const char *str);
/* Free the resources used by a string set */
int spindle_strset_destroy(struct spindle_strset_struct *set);

/* Assert that two URIs are equivalent */
int spindle_proxy_create(SPINDLE *spindle, const char *uri1, const char *uri2, struct spindle_strset_struct *changeset);
/* Generate a new local URI for an external URI */
char *spindle_proxy_generate(SPINDLE *spindle, const char *uri);
/* Look up the local URI for an external URI in the store */
char *spindle_proxy_locate(SPINDLE *spindle, const char *uri);
/* Move a set of references from one proxy to another */
int spindle_proxy_migrate(SPINDLE *spindle, const char *from, const char *to, char **refs);
/* Store a relationship between a proxy and a processed entity */
int spindle_proxy_relate(SPINDLE *spindle, const char *remote, const char *proxy);
/* Obtain all of the outbound references from a proxy */
char **spindle_proxy_refs(SPINDLE *spindle, const char *uri);
/* Destroy a list of references */
void spindle_proxy_refs_destroy(char **refs);

/* Re-build the cached data for a set of proxies */
int spindle_cache_update_set(SPINDLE *spindle, struct spindle_strset_struct *set);
/* Re-build the cached data for the proxy entity identified by localname;
 * if no references exist any more, the cached data will be removed.
 */
int spindle_cache_update(SPINDLE *spindle, const char *localname, struct spindle_strset_struct *set);

/* Determine the class of something (storing in cache->classname) */
int spindle_class_match(SPINDLECACHE *cache, struct spindle_strset_struct *classes);
/* Update the classes of a proxy (updates cache->classname) */
int spindle_class_update(SPINDLECACHE *cache);

/* Update the properties of a proxy */
int spindle_prop_update(SPINDLECACHE *cache);

#endif /*!P_SPINDLE_H_*/
