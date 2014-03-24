/* Twine: SPARQL client helpers
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

#include "p_libtwine.h"

static int twine_sparql_debug;

static char *twine_sparql_query_uri;
static char *twine_sparql_update_uri;
static char *twine_sparql_data_uri;

/* Internal: set defaults for SPARQL connections */
int
twine_sparql_defaults_(const char *query_uri, const char *update_uri, const char *data_uri, int verbose)
{
	if(query_uri)
	{
		twine_sparql_query_uri = strdup(query_uri);
		if(!twine_sparql_query_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL query endpoint is <%s>\n", twine_sparql_query_uri);
	}
	if(update_uri)
	{
		twine_sparql_update_uri = strdup(update_uri);
		if(!twine_sparql_update_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL update endpoint is <%s>\n", twine_sparql_update_uri);
	}
	if(data_uri)
	{
		twine_sparql_data_uri = strdup(data_uri);
		if(!twine_sparql_data_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL RESTful endpoint is <%s>\n", twine_sparql_data_uri);
	}
	twine_sparql_debug = verbose;
	return 0;
}

/* Public: Create a new SPARQL connection */
SPARQL *
twine_sparql_create(void)
{
	SPARQL *p;

	p = sparql_create(NULL);
	if(!p)
	{
		return NULL;
	}
	sparql_set_logger(p, twine_logger_);
	sparql_set_verbose(p, twine_sparql_debug);
	if(twine_sparql_query_uri)
	{
		sparql_set_query_uri(p, twine_sparql_query_uri);
	}
	if(twine_sparql_update_uri)
	{
		sparql_set_update_uri(p, twine_sparql_update_uri);
	}
	if(twine_sparql_data_uri)
	{
		sparql_set_data_uri(p, twine_sparql_data_uri);
	}
	return p;
}

/* Public: Replace a graph from a Turtle buffer */
int
twine_sparql_put(const char *uri, const char *triples, size_t length)
{
	int r;
	SPARQL *conn;
	
	conn = twine_sparql_create();
	if(!conn)
	{
		return -1;
	}
	r = sparql_put(conn, uri, triples, length);
	sparql_destroy(conn);
	return r;
}

/* Public: Replace a graph from a librdf stream */
int
twine_sparql_put_stream(const char *uri, librdf_stream *stream)
{
	char *buf;
	size_t buflen;
	librdf_world *world;
	librdf_serializer *serializer;
	int r;
	SPARQL *conn;

	world = twine_rdf_world();
	serializer = librdf_new_serializer(world, "ntriples", NULL, NULL);
	if(!serializer)
	{
		twine_logf(LOG_ERR, "failed to create ntriples serializer\n");
		return -1;
	}
	buflen = 0;
	buf = (char *) librdf_serializer_serialize_stream_to_counted_string(serializer, NULL, stream, &buflen);
	if(!buf)
	{
		librdf_free_serializer(serializer);
		twine_logf(LOG_ERR, "failed to serialise buffer\n");
		return -1;
	}
	conn = twine_sparql_create();
	if(conn)
	{
		r = sparql_put(conn, uri, buf, buflen);
		sparql_destroy(conn);
	}
	else
	{
		r = -1;
	}
	librdf_free_memory(buf);
	librdf_free_serializer(serializer);
	return r;
}

