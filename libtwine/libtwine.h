/* Twine: Plug-in interface
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
typedef int (*twine_processor_fn)(const char *uri, const char *data, size_t length);

/* Twine plug-in entry-point */
int twine_plugin_init(void);

/* Register a processor callback for a given MIME type */
int twine_plugin_register(const char *mimetype, const char *description, twine_processor_fn fn);

/* Obtain the shared librdf world */
librdf_world *twine_rdf_world(void);

/* Convenience API for creating a new librdf model */
librdf_model *twine_rdf_model_create(void);

/* Parse a buffer into a librdf model */
int twine_rdf_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen);

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
int twine_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value));


# ifdef __cplusplus
}
# endif

#endif /*!TWINE_H_*/
