/* Twine: Plug-in interface
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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
# include <libcluster.h>

# undef BEGIN_DECLS_
# undef END_DECLS_
# undef DEPRECATED_
# undef restrict
# ifdef __cplusplus
#  define BEGIN_DECLS_                  extern "C" {
#  define END_DECLS_                    }
# else
#  define BEGIN_DECLS_
#  define END_DECLS_
# endif
# ifdef __GNUC__
#  define DEPRECATED_                   __attribute__((deprecated))
# else
#  define DEPRECATED_
# endif
# ifdef __cplusplus
#  ifdef __GNUC__
#   define restrict                     __restrict__
#  else
#   define restrict
#  endif
# elif __STDC_VERSION__ < 199901L
#  define restrict
# endif

/* For the moment, this is always defined */
# ifndef TWINE_USE_DEPRECATED_API
#  define TWINE_USE_DEPRECATED_API      1
# endif

BEGIN_DECLS_

typedef struct twine_context_struct TWINE;
typedef struct twine_graph_struct TWINEGRAPH;

/* Plug-in callbacks
 *
 * Functions of this type are invoked by Twine at different points in the
 * processing flow. The order in which they're invoked (apart from the
 * entry-point callback), is defined by the 'workflow' configuration
 * option.
 */

/* Plug-in entry-point: invoked when a plug-in is loaded or un-loaded */

typedef enum
{
	TWINE_ATTACHED,
	TWINE_DETACHED
} TWINEENTRYTYPE;

typedef int (*TWINEENTRYFN)(TWINE *context, TWINEENTRYTYPE type, void *handle);

/* This function must be provided by plug-ins themselves - it is not an API
 * which can be invoked as part of libtwine
 */

int twine_entry(TWINE *restrict context, TWINEENTRYTYPE type, void *restrict handle);

/* Input callbacks process messages of a particular type, and transform them
 * into RDF, storing the result in a graph object.
 */
typedef int (*TWINEINPUTFN)(TWINE *restrict context, const char *restrict mimetype, const unsigned char *restrict data, size_t length, const char *restrict subject, void *userdata);

/* Bulk callbacks are invoked in preference to input callbacks when invoked
 * via a bulk-import process (e.g., the Twine command-line utility) rather
 * than upon receipt of a message.
 */
typedef const unsigned char *(*TWINEBULKFN)(TWINE *restrict context, const char *restrict mimetype, const unsigned char *restrict data, size_t length, void *userdata);

/* Processing callbacks operate on a twine graph object and are responsible
 * for any transformations which should be performed, as well as storing the
 * data somewhere (such as a SPARQL-capable RDF store).
 */
typedef int (*TWINEPROCESSORFN)(TWINE *restrict context, TWINEGRAPH *restrict graph, void *userdata);

/* Update callbacks are registered with a name (typically the module name),
 * and are called with an identifier when invoked by the Twine command-line
 * processing utility. For workflows which involve deriving data or generating
 * indexes, this can be used to trigger a re-build when required.
 */
typedef int (*TWINEUPDATEFN)(TWINE *restrict context, const char *restrict name, const char *restrict identifier, void *userdata);

/* Log events */
void twine_logf(int prio, const char *fmt, ...);
void twine_vlogf(int prio, const char *fmt, va_list ap);

/* Obtain or set configuration settings */
size_t twine_config_get(const char *key, const char *defval, char *buf, size_t bufsize);
char *twine_config_geta(const char *key, const char *defval);
int twine_config_get_int(const char *key, int defval);
int twine_config_get_bool(const char *key, int defval);
int twine_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);
int twine_config_set(const char *key, const char *value);

/* Callback registration */
int twine_plugin_add_input(TWINE *restrict context, const char *restrict mimetype, const char *restrict description, TWINEINPUTFN fn, void *userdata);
int twine_plugin_input_exists(TWINE *restrict context, const char *mimetype);
int twine_plugin_add_bulk(TWINE *restrict context, const char *restrict mimetype, const char *restrict description, TWINEBULKFN fn, void *userdata);
int twine_plugin_bulk_exists(TWINE *restrict context, const char *mimetype);
int twine_plugin_add_processor(TWINE *restrict context, const char *restrict name, TWINEPROCESSORFN fn, void *userdata);
int twine_plugin_processor_exists(TWINE *restrict context, const char *restrict name);
int twine_plugin_add_update(TWINE *restrict context, const char *restrict name, TWINEUPDATEFN fn, void *userdata);
int twine_plugin_update_exists(TWINE *restrict context, const char *restrict name);

/* Graph objects */
TWINEGRAPH *twine_graph_create(TWINE *restrict context, const char *restrict uri);
TWINEGRAPH *twine_graph_create_rdf(TWINE *restrict context, const char *restrict uri, const unsigned char *restrict buf, size_t buflen, const char *restrict type);
int twine_graph_destroy(TWINEGRAPH *graph);
const char *twine_graph_uri(TWINEGRAPH *graph);
librdf_model *twine_graph_model(TWINEGRAPH *graph);
librdf_model *twine_graph_orig_model(TWINEGRAPH *graph);

/* Workflow processing */
int twine_workflow_process_message(TWINE *restrict context, const char *restrict mimetype, const unsigned char *restrict message, size_t messagelen, const char *restrict subject);
int twine_workflow_process_file(TWINE *restrict context, const char *restrict mimetype, FILE *restrict file);
int twine_workflow_process_graph(TWINE *restrict context, TWINEGRAPH *restrict graph);
int twine_workflow_process_update(TWINE *restrict context, const char *restrict type, const char *restrict id);
int twine_workflow_process_rdf(TWINE *restrict context, const char *restrict uri, const unsigned char *restrict buf, size_t buflen, const char *restrict type);
int twine_workflow_process_stream(TWINE *restrict context, const char *restrict uri, librdf_stream *stream);

/* RDF utility wrappers */

/* Obtain the shared librdf world */
librdf_world *twine_rdf_world(void);

/* Convenience API for creating a new librdf model */
librdf_model *twine_rdf_model_create(void);

/* Convenience API for cloning librdf model */
librdf_model *twine_rdf_model_clone(librdf_model *model);

/* Destroy a model */
int twine_rdf_model_destroy(librdf_model *model);

/* Parse a buffer into a librdf model */
int twine_rdf_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen);
int twine_rdf_model_parse_graph(librdf_model *model, const char *mime, const char *buf, size_t buflen, librdf_node *graph);
int twine_rdf_model_parse_base(librdf_model *model, const char *mime, const char *buf, size_t buflen, librdf_uri *uri);
int twine_rdf_model_parse_base_graph(librdf_model *model, const char *mime, const char *buf, size_t buflen, librdf_uri *base, librdf_node *graph);

/* Add a statement to a model if it doesn't exist */
int twine_rdf_model_add_st(librdf_model *model, librdf_statement *statement, librdf_node *ctx);
/* Add a stream to a model, provided the statements don't already exist */
int twine_rdf_model_add_stream(librdf_model *model, librdf_stream *stream, librdf_node *ctx);
/* Create a new statement */
librdf_statement *twine_rdf_st_create(void);

/* Duplicate a statement */
librdf_statement *twine_rdf_st_clone(librdf_statement *src);

/* Destroy a statement */
int twine_rdf_st_destroy(librdf_statement *statement);

/* Obtain the integer value of the object of a statement */
int twine_rdf_st_obj_intval(librdf_statement *statement, long *value);

/* Clone a node */
librdf_node *twine_rdf_node_clone(librdf_node *node);

/* Create a new URI node */
librdf_node *twine_rdf_node_createuri(const char *uri);

/* Create a new literal */
librdf_node *twine_rdf_node_createliteral(const char *literal);

/* Destroy a node */
int twine_rdf_node_destroy(librdf_node *node);

/* Check if a node is an integer type */
int twine_rdf_node_isint(librdf_node *node);

/* Obtain the integer value of a node */
int twine_rdf_node_intval(librdf_node *node, long *value);

/* Serialise a model to a string */
char *twine_rdf_model_ntriples(librdf_model *model, size_t *buflen);
char *twine_rdf_model_nquads(librdf_model *model, size_t *buflen);

/* Serialise a stream to a string */
char *twine_rdf_stream_ntriples(librdf_stream *model, size_t *buflen);

/* SPARQL handling */

/* Create a SPARQL connection */
SPARQL *twine_sparql_create(void);

/* Clustering */
CLUSTER *twine_cluster(TWINE *context);
int twine_cluster_enable(TWINE *context, int enabled);

/* The interface defined below is now considered legacy. It will continue to
 * be provided for binary compatibility, but plug-ins built from source must
 * define TWINE_USE_DEPRECATED_API in order for the definitions to be visible.
 *
 * Eventually, source-level compatibility will be removed.
 */
# ifdef TWINE_USE_DEPRECATED_API

typedef struct twine_graph_struct twine_graph;

struct twine_graph_struct
{
	/* The graph URI */
	char *uri;
	void *reserved;
	/* The new graph, possibly modified by processors */
	librdf_model *store;
	/* The old graph in the quad store, if available */
	librdf_model *old;
};

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

/* A Twine graph-processing callback */
typedef int (*twine_graph_fn)(twine_graph *graph, void *userdata);

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

/* A Twine update callback
 *
 * This callback is invoked exclusively by the 'twine' command-line utility
 * in order to request that a plug-in update its caches (or other ancilliary
 * data) for the specified identifier. The format of the identifier is
 * plug-in-specific, and its structure is a matter for the plug-in and the
 * user to agree upon (typically, it would be a URI or a UUID of something
 * generated or processed by the plug-in).
 */
typedef int (*twine_update_fn)(const char *name, const char *identifier, void *userdata);

/* Twine plug-in entry-point (this is not an API, but the signature for the function
 * provided by a plug-in)
 */
int twine_plugin_init(void) DEPRECATED_;

/* Twine plug-in clean-up entry-point (this is not an API, but the signature for the
 * function provided by a plug-in)
 */
int twine_plugin_done(void) DEPRECATED_;

/* Obtain the path to the Twine configuration file */
const char *twine_config_path(void) DEPRECATED_;

/* Obtain the default URI of the message broker */
const char *twine_mq_default_uri(void) DEPRECATED_;

/* Register a processor callback for a given MIME type */
int twine_plugin_register(const char *mimetype, const char *description, twine_processor_fn fn, void *data) DEPRECATED_;

/* Register a bulk processor callback for a given MIME type */
int twine_bulk_register(const char *mimetype, const char *description, twine_bulk_fn fn, void *data) DEPRECATED_;

/* Check whether a MIME type is supported by any registered processor */
int twine_plugin_supported(const char *mimetype) DEPRECATED_;
int twine_bulk_supported(const char *mimetype) DEPRECATED_;
int twine_update_supported(const char *name) DEPRECATED_;
int twine_graph_supported(const char *name) DEPRECATED_;

/* Process a single message */
int twine_plugin_process(const char *mimetype, const unsigned char *message, size_t msglen, const char *subject) DEPRECATED_;

/* Ask a named plug-in to update a resource */
int twine_update(const char *plugin, const char *identifier) DEPRECATED_;

/* Register a graph processor */
int twine_graph_register(const char *name, twine_graph_fn fn, void *data) DEPRECATED_;

/* Deprecated (see twine_graph_register): register a pre-processor */
int twine_preproc_register(const char *name, twine_preproc_fn fn, void *data) DEPRECATED_;

/* Deprecated (see twine_graph_register): register a post-processor */
int twine_postproc_register(const char *name, twine_postproc_fn fn, void *data) DEPRECATED_;

/* Register an update handler */
int twine_update_register(const char *name, twine_update_fn fn, void *data) DEPRECATED_;


/* Replace a graph with supplied triples */
int twine_sparql_put(const char *uri, const char *triples, size_t length) DEPRECATED_;
int twine_sparql_put_format(const char *uri, const char *triples, size_t length, const char *type) DEPRECATED_;

/* Replace a graph contained within a librdf stream */
int twine_sparql_put_stream(const char *uri, librdf_stream *stream) DEPRECATED_;

/* Replace a graph contained within a librdf model */
int twine_sparql_put_model(const char *uri, librdf_model *model) DEPRECATED_;

# endif /*TWINE_USE_DEPRECATED_API*/

END_DECLS_

#endif /*!TWINE_H_*/
