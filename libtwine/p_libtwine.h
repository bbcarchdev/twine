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

# include <stdlib.h>
# include <dlfcn.h>
# include <errno.h>
# include <string.h>
# include <curl/curl.h>

# include "libtwine-internal.h"

# define DEFAULT_MQ_URI               "amqp://~0.0.0.0/amq.direct"

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
	TCB_MIME,
	TCB_BULK,
	TCB_PREPROC,
	TCB_POSTPROC,
	TCB_UPDATE,
	TCB_GRAPH
} twine_callback_type;

struct twine_mime_struct
{
	char *type;
	char *desc;
	twine_processor_fn fn;
};

struct twine_bulk_struct
{
	char *type;
	char *desc;
	twine_bulk_fn fn;
};

struct twine_postproc_struct
{
	twine_postproc_fn fn;
	char *name;
};

struct twine_preproc_struct
{
	twine_preproc_fn fn;
	char *name;
};

struct twine_graphproc_struct
{
	twine_preproc_fn fn;
	char *name;
};

struct twine_update_struct
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
		struct twine_mime_struct mime;
		struct twine_bulk_struct bulk;
		struct twine_preproc_struct preproc;
		struct twine_postproc_struct postproc;
		struct twine_update_struct update;
		struct twine_graphproc_struct graph;
	} m;
};

extern twine_log_fn twine_logger_;

int twine_log_init_(twine_log_fn logfn);
int twine_log_cleanup_(void);
int twine_rdf_init_(void);
int twine_rdf_cleanup_(void);
int twine_graph_cleanup_(twine_graph *graph);
int twine_plugin_unload_all_(void);
int twine_workflow_init_(void);
int twine_workflow_process_(twine_graph *graph);

#endif /*!P_LIBTWINE_H_*/
