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

#ifndef LIBTWINE_H_
# define LIBTWINE_H_                    1

# include <stdarg.h>
# include <librdf.h>
# include <syslog.h>
# include <libsparqlclient.h>

# ifdef __cplusplus
extern "C" {
# endif

/* A Twine processor callback
 *
 * This callback is invoked when a message of the MIME type registered
 * for this plug-in is received.
 */
typedef int (*twine_processor_fn)(const char *mimetype, const unsigned char *data, size_t length, void *userdata);


/* A Twine bulk processor callback
 *
 * This process is invoked (in preference to any ordinary processor) in order
 * to ingest data as part of a bulk import process.
 *
 * The function should return NULL to indicate an error has occurred,
 * or a pointer to the current 'start of next data element' (which might be
 * the trailing null byte) if all ingestable data in the buffer so far has
 * been processed.
 *
 */
typedef const unsigned char *(*twine_bulk_fn)(const char *mimetype, const unsigned char *data, size_t length, void *userdata);

/* Graph processing */

typedef struct twine_graph_struct twine_graph;

struct twine_graph_struct
{
	/* The graph URI */
	const char *uri;
	/* The new graph, as originally supplied to the processing chain */
	librdf_model *pristine;
	/* The new graph, possibly modified by processors (initially a clone
	 * of pristine
	 */
	librdf_model *store;
	/* The old graph in the quad store, if available */
	librdf_model *old;
};

/* A Twine pre-processing callback
 *
 * This callback is invoked before a graph is added, removed, or modified
 */
typedef int (*twine_preproc_fn)(twine_graph *graph, void *userdata);

/* A Twine post-processing callback
 *
 * This callback is invoked after a graph has been added, removed, or modified
 */
typedef int (*twine_postproc_fn)(twine_graph *graph, void *userdata);

/* Twine plug-in entry-point */
int twine_plugin_init(void);

/* Register a processor callback for a given MIME type */
int twine_plugin_register(const char *mimetype, const char *description, twine_processor_fn fn, void *data);

/* Register a bulk processor callback for a given MIME type */
int twine_bulk_register(const char *mimetype, const char *description, twine_bulk_fn fn, void *data);

/* Check whether a MIME type is supported by any registered processor */
int twine_plugin_supported(const char *mimetype);
int twine_bulk_supported(const char *mimetype);

/* Process a single message */
int twine_plugin_process(const char *mimetype, const unsigned char *message, size_t msglen, const char *subject);

/* Perform a bulk import from a file */
int twine_bulk_import(const char *mimetype, FILE *file);

/* Register a post-processor */
int twine_postproc_register(const char *name, twine_postproc_fn fn, void *data);

/* Obtain the shared librdf world */
librdf_world *twine_rdf_world(void);

/* Convenience API for creating a new librdf model */
librdf_model *twine_rdf_model_create(void);

/* Destroy a model */
int twine_rdf_model_destroy(librdf_model *model);

/* Parse a buffer into a librdf model */
int twine_rdf_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen);
	int twine_rdf_model_parse_base(librdf_model *model, const char *mime, const char *buf, size_t buflen, librdf_uri *uri);

/* Create a new statement */
librdf_statement *twine_rdf_st_create(void);

/* Duplicate a statement */
librdf_statement *twine_rdf_st_clone(librdf_statement *src);

/* Destroy a statement */
int twine_rdf_st_destroy(librdf_statement *statement);

/* Clone a node */
librdf_node *twine_rdf_node_clone(librdf_node *node);

/* Create a new URI node */
librdf_node *twine_rdf_node_createuri(const char *uri);

/* Destroy a node */
int twine_rdf_node_destroy(librdf_node *node);

/* Serialise a model to a string */
char *twine_rdf_model_ntriples(librdf_model *model, size_t *buflen);

/* Serialise a stream to a string */
char *twine_rdf_stream_ntriples(librdf_stream *model, size_t *buflen);

/* Create a SPARQL connection */
SPARQL *twine_sparql_create(void);

/* Replace a graph with supplied triples */
int twine_sparql_put(const char *uri, const char *triples, size_t length);

/* Replace a graph contained within a librdf stream */
int twine_sparql_put_stream(const char *uri, librdf_stream *stream);

/* Log an event */
void twine_logf(int prio, const char *fmt, ...);
void twine_vlogf(int prio, const char *fmt, va_list ap);

/* Obtain configuration values */
size_t twine_config_get(const char *key, const char *defval, char *buf, size_t bufsize);
char *twine_config_geta(const char *key, const char *defval);
int twine_config_get_int(const char *key, int defval);
int twine_config_get_bool(const char *key, int defval);
int twine_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);


# ifdef __cplusplus
}
# endif

#endif /*!TWINE_H_*/
