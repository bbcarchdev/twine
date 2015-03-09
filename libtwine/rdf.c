/* Twine: RDF helpers
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

#include "p_libtwine.h"

#ifndef LIBRDF_MODEL_FEATURE_CONTEXTS
# error librdf library version is too old; please upgrade to a version which supports contexts
#endif

static librdf_world *twine_world;

static int twine_librdf_logger(void *data, librdf_log_message *message);

int
twine_rdf_init_(void)
{
	twine_world = librdf_new_world();
	if(!twine_world)
	{
		twine_logf(LOG_CRIT, "failed to create new RDF world\n");
		return -1;
	}
	librdf_world_open(twine_world);
	librdf_world_set_logger(twine_world, NULL, twine_librdf_logger);
	return 0;
}

librdf_world *
twine_rdf_world(void)
{
	return twine_world;
}

/* Create a new model */
librdf_model *
twine_rdf_model_create(void)
{
	librdf_model *model;
	librdf_storage *storage;
	
	storage = librdf_new_storage(twine_world, "hashes", NULL, "hash-type='memory',contexts='yes'");
	if(!storage)
	{
		twine_logf(LOG_CRIT, "failed to create new RDF storage\n");
		return NULL;
	}
	model = librdf_new_model(twine_world, storage, NULL);
	if(!model)
	{
		twine_logf(LOG_CRIT, "failed to create new RDF model\n");
		librdf_free_storage(storage);
		return NULL;
	}
	return model;
}

librdf_model *
twine_rdf_model_clone(librdf_model *model)
{
	librdf_model *dest;
	char *nq;
	size_t nqlen;
	
	dest = twine_rdf_model_create();
	if(!dest)
	{
		return NULL;
	}
	if(!(nq = twine_rdf_model_nquads(model, &nqlen)))
	{
		twine_rdf_model_destroy(dest);
		return NULL;
	}	   
	if(twine_rdf_model_parse(dest, "application/n-quads", nq, nqlen))
	{
		librdf_free_memory(nq);
		twine_rdf_model_destroy(dest);
		return NULL;
	}
	librdf_free_memory(nq);
	return dest;
}

/* Destroy a model */
int
twine_rdf_model_destroy(librdf_model *model)
{
	if(model)
	{
		librdf_free_model(model);
	}
	return 0;
}

static int
nstrcasecmp(const char *a, const char *b, size_t alen)
{
	if(strlen(b) != alen)
	{
		return -1;
	}
	return strncasecmp(a, b, alen);
}

/* Parse a buffer of a particular MIME type into a model */
int
twine_rdf_model_parse_base(librdf_model *model, const char *mime, const char *buf, size_t buflen, librdf_uri *base)
{
	const char *name, *t;
	librdf_parser *parser;
	int r, sl;

	t = strchr(mime, ';');
	if(t)
	{
		sl = t - mime;
	}
	else
	{
		sl = strlen(mime);
	}
	name = NULL;
	/* Handle specific MIME types whether or not librdf already knows
	 * about them
	 */
	if(!nstrcasecmp(mime, "application/trig", sl))
	{
		name = "trig";
	}
	else if(!nstrcasecmp(mime, "application/n-quads", sl) || !nstrcasecmp(mime, "text/x-nquads", sl))
	{
		name = "nquads";
	}
	else if(!nstrcasecmp(mime, "application/n-triples", sl) || !nstrcasecmp(mime, "text/plain", sl))
	{
		name = "ntriples";
	}
	else if(!nstrcasecmp(mime, "text/turtle", sl) || !nstrcasecmp(mime, "text/n3", sl))
	{
		name = "turtle";
	}
	else if(!nstrcasecmp(mime, "application/rdf+xml", sl))
	{
		name = "rdfxml";
	}
	/* If we have a specific parser name, don't use the MIME type */
	if(name)
	{
		mime = NULL;
	}
	parser = librdf_new_parser(twine_world, name, mime, NULL);
	if(!parser)
	{
		if(!name)
		{
			name = "auto";
		}
		twine_logf(LOG_ERR, "failed to create a new parser for %s (%s)\n", mime, name);
		return -1;
	}
	r = librdf_parser_parse_counted_string_into_model(parser, (const unsigned char *) buf, buflen, base, model);
	if(r)
	{
		twine_logf(LOG_DEBUG, "failed to parse buffer of %u bytes as %s\n", (unsigned int) buflen, mime ? mime : name);
	}
	librdf_free_parser(parser);
	return r;	
}

/* Parse a buffer of a particular MIME type into a model */
int
twine_rdf_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen)
{	
	static librdf_uri *base;
	
	if(!base)
	{
		base = librdf_new_uri(twine_world, (const unsigned char *) "/");
		if(!base)
		{
			twine_logf(LOG_CRIT, "failed to parse URI </>\n");
			return -1;
		}
	}
	return twine_rdf_model_parse_base(model, mime, buf, buflen, base);
}

/* Add a statement to a model, provided it doesn't already exist */
int
twine_rdf_model_add_st(librdf_model *model, librdf_statement *statement, librdf_node *ctx)
{
	librdf_stream *st;

	if(ctx)
	{
		st = librdf_model_find_statements_with_options(model, statement, ctx, NULL);
	}
	else
	{
		st = librdf_model_find_statements(model, statement);
	}
	if(!librdf_stream_end(st))
	{
		librdf_free_stream(st);
		return 0;
	}
	if(ctx)
	{
		return librdf_model_context_add_statement(model, ctx, statement);
	}
	return librdf_model_add_statement(model, statement);
}

/* Add a stream to a model, provided the statements don't already exist */
int
twine_rdf_model_add_stream(librdf_model *model, librdf_stream *stream, librdf_node *ctx)
{
	for(; !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		if(twine_rdf_model_add_st(model, librdf_stream_get_object(stream), ctx))
		{
			return -1;
		}
	}
	return 0;
}

/* Create a new statement */
librdf_statement *
twine_rdf_st_create(void)
{
	librdf_statement *st;

	st = librdf_new_statement(twine_world);
	if(!st)
	{
		twine_logf(LOG_ERR, "failed to create new statement\n");
		return NULL;
	}
	return st;
}

/* Duplicate a statement */
librdf_statement *
twine_rdf_st_clone(librdf_statement *src)
{
	librdf_statement *st;

	st = librdf_new_statement_from_statement(src);
	if(!st)
	{
		twine_logf(LOG_ERR, "failed to clone statement\n");
		return NULL;
	}
	return st;
}

/* Destroy a statement */
int
twine_rdf_st_destroy(librdf_statement *statement)
{
	if(statement)
	{
		librdf_free_statement(statement);
	}
	return 0;
}

int
twine_rdf_st_obj_intval(librdf_statement *statement, long *value)
{
	librdf_node *node;

	node = librdf_statement_get_object(statement);
	if(!node)
	{
		return 0;
	}
	return twine_rdf_node_intval(node, value);
}

/* Clone a node */
librdf_node *
twine_rdf_node_clone(librdf_node *node)
{
	librdf_node *p;

	p = librdf_new_node_from_node(node);
	if(!p)
	{
		twine_logf(LOG_ERR, "failed to clone node\n");
		return NULL;
	}
	return p;
}

/* Create a new URI node */
librdf_node *
twine_rdf_node_createuri(const char *uri)
{
	librdf_node *p;

	p = librdf_new_node_from_uri_string(twine_world, (const unsigned char *) uri);
	if(!p)
	{
		twine_logf(LOG_ERR, "failed to create new node from <%s>\n", uri);
		return NULL;
	}
	return p;
}

/* Destroy a node */
int
twine_rdf_node_destroy(librdf_node *node)
{
	if(node)
	{
		librdf_free_node(node);
	}
	return 0;
}

/* Check if a node's datatype URI is an integer */
int
twine_rdf_node_isint(librdf_node *node)
{
	librdf_uri *uri;
	const char *dtstr;

	uri = librdf_node_get_literal_value_datatype_uri(node);
	if(!uri)
	{
		return 0;
	}
	dtstr = (const char *) librdf_uri_as_string(uri);
	if(strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#integer") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#long") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#short") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#byte") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#int") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#nonPositiveInteger") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#nonNegativeInteger") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#negativeInteger") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#positiveInteger") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedLong") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedInt") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedShort") &&
	   strcmp(dtstr, "http://www.w3.org/2001/XMLSchema#unsignedByte"))
	{
		return 0;
	}
	return 1;
}

/* Get the integer value of a node */
int
twine_rdf_node_intval(librdf_node *node, long *value)
{
	const char *str;
	char *endp;

	if(!librdf_node_is_literal(node))
	{
		return 0;
	}
	if(!twine_rdf_node_isint(node))
	{
		return 0;
	}
	str = (const char *) librdf_node_get_literal_value(node);
	if(!str || !*str)
	{
		return 0;
	}
	endp = NULL;
	*value = strtol(str, &endp, 10);
	if(!endp || !*endp)
	{
		return 1;
	}
	return 0;
}

/* Log events from librdf */
static int
twine_librdf_logger(void *data, librdf_log_message *message)
{
	int level;

	(void) data;
	
	switch(librdf_log_message_level(message))
	{
	case LIBRDF_LOG_DEBUG:
		level = LOG_DEBUG;
		break;
	case LIBRDF_LOG_INFO:
		level = LOG_INFO;
		break;
	case LIBRDF_LOG_WARN:
		level = LOG_WARNING;
		break;
	case LIBRDF_LOG_ERROR:
		level = LOG_ERR;
		break;
	case LIBRDF_LOG_FATAL:
		level = LOG_CRIT;
		break;
	default:
		level = LOG_NOTICE;
		break;
	}
	twine_logf(level, "RDF: %s\n", librdf_log_message_message(message));
	return 0;
}

/* Serialise a model to a string - the result should be freed by
 * librdf_free_memory()
 */
char *
twine_rdf_model_ntriples(librdf_model *model, size_t *buflen)
{
	char *buf;
	librdf_world *world;
	librdf_serializer *serializer;

	*buflen = 0;
	world = twine_rdf_world();
	serializer = librdf_new_serializer(world, "ntriples", NULL, NULL);
	if(!serializer)
	{
		twine_logf(LOG_ERR, "failed to create ntriples serializer\n");
		return NULL;
	}
	buf = (char *) librdf_serializer_serialize_model_to_counted_string(serializer, NULL, model, buflen);
	if(!buf)
	{
		librdf_free_serializer(serializer);
		twine_logf(LOG_ERR, "failed to serialise model to buffer\n");
		return NULL;
	}
	librdf_free_serializer(serializer);
	return buf;
}

/* Serialise a model to a string - the result should be freed by
 * librdf_free_memory()
 */
char *
twine_rdf_model_nquads(librdf_model *model, size_t *buflen)
{   
	*buflen = 0;
	return (char *) librdf_model_to_counted_string(model, NULL, "nquads", NULL, NULL, buflen);
}

/* Serialise a stream to a string - the result should be freed by
 * librdf_free_memory()
 */
char *
twine_rdf_stream_ntriples(librdf_stream *stream, size_t *buflen)
{
	char *buf;
	librdf_world *world;
	librdf_serializer *serializer;

	*buflen = 0;
	world = twine_rdf_world();
	serializer = librdf_new_serializer(world, "ntriples", NULL, NULL);
	if(!serializer)
	{
		twine_logf(LOG_ERR, "failed to create ntriples serializer\n");
		return NULL;
	}
	buf = (char *) librdf_serializer_serialize_stream_to_counted_string(serializer, NULL, stream, buflen);
	if(!buf)
	{
		librdf_free_serializer(serializer);
		twine_logf(LOG_ERR, "failed to serialise stream to buffer\n");
		return NULL;
	}
	librdf_free_serializer(serializer);
	return buf;
}

int
twine_graph_cleanup_(twine_graph *graph)
{
	if(graph->old)
	{
		librdf_free_model(graph->old);
	}
	if(graph->pristine)
	{
		librdf_free_model(graph->pristine);
	}
	if(graph->store)
	{
		librdf_free_model(graph->store);
	}
	return 0;
}

