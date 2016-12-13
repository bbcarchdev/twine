/* Twine: Plug-in interface
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

#ifndef P_LIBTWINE_H_
# define P_LIBTWINE_H_                  1

# include <stdio.h>
# include <stdlib.h>
# include <ctype.h>
# include <dlfcn.h>
# include <errno.h>
# include <string.h>
# include <pthread.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/param.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <curl/curl.h>
# include <libcluster.h>
# include <libawsclient.h>
# include <libsql.h>

# include "libsupport.h"

# define TWINE_USE_DEPRECATED_API       1

# include "libtwine-internal.h"

# define DEFAULT_MQ_URI                 "amqp://~0.0.0.0/amq.direct"

# define DEFAULT_CONFIG_PATH            SYSCONFDIR "/twine.conf"
# define DEFAULT_CONFIG_SECTION_NAME    "twine"
# define DEFAULT_CONFIG_SECTION         DEFAULT_CONFIG_SECTION_NAME ":"
# define DEFAULT_CONFIG_SECTION_LEN     6

# define MIME_TURTLE                    "text/turtle"
# define MIME_NTRIPLES                  "application/n-triples"
# define MIME_NQUADS                    "application/n-quads"
# define MIME_NQUADS_OLD                "text/x-nquads"
# define MIME_RDFXML                    "application/rdf+xml"
# define MIME_TRIG                      "application/trig"
# define MIME_PLAIN                     "text/plain"
# define MIME_N3                        "text/n3"

typedef int (*twine_plugin_init_fn)(void);
typedef int (*twine_plugin_cleanup_fn)(void);

typedef enum
{
	TCB_NONE,
	TCB_INPUT,
	TCB_BULK,
	TCB_UPDATE,
	TCB_PROCESSOR,
	/* Legacy callback types */
	TCB_LEGACY_MIME,
	TCB_LEGACY_BULK,
	TCB_LEGACY_UPDATE,
	TCB_LEGACY_GRAPH	
} twine_callback_type;

struct twine_legacy_mime_struct
{
	char *type;
	char *desc;
	twine_processor_fn fn;
};

struct twine_legacy_bulk_struct
{
	char *type;
	char *desc;
	twine_bulk_fn fn;
};

struct twine_legacy_graphproc_struct
{
	twine_preproc_fn fn;
	char *name;
};

struct twine_legacy_update_struct
{
	twine_update_fn fn;
	char *name;
};

struct twine_callback_struct
{
	twine_callback_type type;
	void *module;
	void *data;
	union
	{
		struct
		{
			char *type;
			char *desc;
			TWINEINPUTFN fn;
		} input;

		struct
		{
			char *type;
			char *desc;
			TWINEBULKFN fn;
		} bulk;

		struct
		{
			char *name;
			TWINEPROCESSORFN fn;
		} processor;

		struct
		{
			char *name;
			TWINEUPDATEFN fn;
		} update;
			
		struct twine_legacy_mime_struct legacy_mime;
		struct twine_legacy_bulk_struct legacy_bulk;
		struct twine_legacy_update_struct legacy_update;
		struct twine_legacy_graphproc_struct legacy_graph;
	} m;
};

struct twine_context_struct
{
	TWINE *prev;
	TWINELOGFN logger;
	librdf_world *world;
	TWINECONFIGFNS config;
	char *appname;
	size_t appnamelen;
	char *keybuf;
	size_t keybuflen;
	int sparql_debug;
	char *sparql_uri;
	char *sparql_query_uri;
	char *sparql_update_uri;
	char *sparql_data_uri;
	int allow_internal;
	int is_daemon;
	int plugins_enabled;
	CLUSTER *cluster;
	int cluster_enabled;
	void *plugin_current;
	struct twine_callback_struct *callbacks;
	size_t cbcount;
	size_t cbsize;
	/* The RDBMS connection */
	SQL *db;
};

extern TWINE *twine_;

int twine_rdf_init_(TWINE *context);
int twine_rdf_cleanup_(TWINE *context);

int twine_graph_cleanup_(twine_graph *graph);
int twine_graph_process_(const char *name, twine_graph *graph);

int twine_plugin_init_(TWINE *context);
int twine_plugin_unload_all_(TWINE *context);
int twine_plugin_allow_internal_(TWINE *context, int);
struct twine_callback_struct *twine_plugin_callback_add_(TWINE *context, void *data);

int twine_workflow_init_(TWINE *context);
int twine_workflow_process_(twine_graph *graph);

int twine_preproc_process_(twine_graph *graph);
int twine_postproc_process_(twine_graph *graph);

int twine_sparql_init_(TWINE *context);

int twine_cache_store_s3_(const char *g, char *ntbuf, size_t bufsize);
int twine_cache_fetch_s3_(const char *g, char **ntbuf, size_t *buflen);
int twine_cache_index_subject_objects_(TWINE *restrict context, TWINEGRAPH *restrict graph);
int twine_cache_index_media_(TWINE *restrict context, TWINEGRAPH *restrict graph);
int twine_cache_fetch_graph_(librdf_model *model, const char *uri);
int twine_cache_fetch_about_(librdf_model *model, const char *uri);


int twine_db_init_(TWINE *context);
int twine_db_schema_update_(TWINE *twine);

int twine_cluster_init_(TWINE *context);
int twine_cluster_ready_(TWINE *context);
int twine_cluster_done_(TWINE *context);

/* This can't be called twine_config_init_() because that's a legacy API */
int twine_config_setup_(TWINE *context);
int twine_config_ready_(TWINE *context);

#endif /*!P_LIBTWINE_H_*/
