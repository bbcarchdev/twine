/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015 BBC
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

static char *spindle_loadfile_(const char *filename);
static int spindle_class_compare_(const void *ptra, const void *ptrb);
static struct spindle_classmatch_struct *spindle_class_add_(SPINDLE *spindle, const char *uri);
static int spindle_class_add_match_(struct spindle_classmatch_struct *match, const char *uri);
static int spindle_class_add_statement_(SPINDLE *spindle, librdf_model *model, librdf_statement *statement);
static int spindle_class_set_score_(struct spindle_classmatch_struct *entry, librdf_statement *statement);

int
spindle_rulebase_init(SPINDLE *spindle)
{
	char *rulebase, *buf;
	librdf_model *model;
	librdf_stream *stream;
	librdf_statement *statement;
	size_t c, d;

	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create new RDF model\n");
		return -1;
	}
	rulebase = twine_config_geta("spindle:rulebase", LIBDIR "/" PACKAGE_TARNAME "/rulebase.ttl");
	buf = spindle_loadfile_(rulebase);
	if(!buf)
	{
		/* spindle_loadfile() will log errors by itself */
		librdf_free_model(model);
		free(rulebase);
		return -1;
	}
	free(rulebase);
	if(twine_rdf_model_parse(model, "text/turtle", buf, strlen(buf)))
	{
		twine_logf(LOG_CRIT, "failed to parse Spindle rulebase as Turtle\n");
		free(buf);
		librdf_free_model(model);
		return -1;
	}
	free(buf);
	/* Parse the model, adding data to the match list */
	stream = librdf_model_as_stream(model);
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		spindle_class_add_statement_(spindle, model, statement);
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_model(model);
	/* Sort the class-match information */
	qsort(spindle->classes, spindle->classcount, sizeof(struct spindle_classmatch_struct), spindle_class_compare_);
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": classes rule-base (%d entries):\n", (int) spindle->classcount);
	for(c = 0; c < spindle->classcount; c++)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s>\n", spindle->classes[c].score, spindle->classes[c].uri);
		for(d = 0; spindle->classes[c].match && spindle->classes[c].match[d]; d++)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME "  +--> <%s>\n", spindle->classes[c].match[d]);
		}
	}
	return 0;
}


static char *
spindle_loadfile_(const char *filename)
{
	char *buffer, *p;
	size_t bufsize, buflen;
	ssize_t r;
	FILE *f;

	buffer = NULL;
	bufsize = 0;
	buflen = 0;
	f = fopen(filename, "rb");
	if(!f)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": %s: %s\n", filename, strerror(errno));
		return NULL;
	}
	while(!feof(f))
	{
		if(bufsize - buflen < 1024)
		{
			p = (char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				return NULL;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, f);
		if(r < 0)
		{
			twine_logf(LOG_CRIT, "error reading from %s: %s\n", filename, strerror(errno));
			free(buffer);
			return NULL;
		}
		buflen += r;
		buffer[buflen] = 0;
	}
	fclose(f);
	return buffer;
}

static int
spindle_class_compare_(const void *ptra, const void *ptrb)
{
	struct spindle_classmatch_struct *a, *b;
	
	a = (struct spindle_classmatch_struct *) ptra;
	b = (struct spindle_classmatch_struct *) ptrb;
	
	return a->score - b->score;
}

/* Note that the return value from this function is only valid until the next
 * call, because it may reallocate the block of memory into which it points
 */
static struct spindle_classmatch_struct *
spindle_class_add_(SPINDLE *spindle, const char *uri)
{
	size_t c;
	struct spindle_classmatch_struct *p;

	for(c = 0; c < spindle->classcount; c++)
	{
		if(!strcmp(uri, spindle->classes[c].uri))
		{
			return &(spindle->classes[c]);
		}
	}
	if(spindle->classcount + 1 >= spindle->classsize)
	{
		p = (struct spindle_classmatch_struct *) realloc(spindle->classes, sizeof(struct spindle_classmatch_struct) * (spindle->classsize + 4));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand class-match list\n");
			return NULL;
		}
		spindle->classes = p;
		spindle->classsize += 4;
	}
	p = &(spindle->classes[spindle->classcount]);
	memset(p, 0, sizeof(struct spindle_classmatch_struct));
	p->uri = strdup(uri);
	if(!p->uri)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate class URI\n");
		return NULL;
	}
	p->score = 100;
	spindle->classcount++;
	return p;
}

static int
spindle_class_add_match_(struct spindle_classmatch_struct *match, const char *uri)
{
	size_t c;
	char **p;

	for(c = 0; c < match->matchsize; c++)
	{
		if(match->match[c])
		{
			if(!strcmp(match->match[c], uri))
			{
				return 0;
			}
		}
		else
		{
			break;
		}
	}
	if(c >= match->matchsize)
	{
		p = (char **) realloc(match->match, sizeof(char *) * (match->matchsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand match URI list\n");
			return -1;
		}
		match->match = p;
		memset(&(p[match->matchsize]), 0, sizeof(char *) * (4 + 1));
		match->matchsize += 4;
	}
	match->match[c] = strdup(uri);
	if(!match->match[c])
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate match URI\n");
		return -1;
	}
	return 1;
}

static int
spindle_class_set_score_(struct spindle_classmatch_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *dt;
	const char *dturi;
	int score;

	object = librdf_statement_get_object(statement);			
	if(!librdf_node_is_literal(object))
	{
		return 0;
	}
	dt = librdf_node_get_literal_value_datatype_uri(object);
	if(!dt)
	{
		return 0;
	}
	dturi = (const char *) librdf_uri_as_string(dt);
	if(strcmp(dturi, "http://www.w3.org/2001/XMLSchema#integer"))
	{
		return 0;
	}
	score = atoi((const char *) librdf_node_get_literal_value(object));
	if(score <= 0)
	{
		return 0;
	}
	entry->score = score;
	return 1;
}

static int
spindle_class_add_statement_(SPINDLE *spindle, librdf_model *model, librdf_statement *statement)
{
	librdf_node *subject, *predicate, *object, *s, *p;
	librdf_uri *subj, *pred, *obj;
	librdf_statement *query, *st;
	librdf_stream *stream;
	const char *subjuri, *preduri, *objuri;
	int r;
	struct spindle_classmatch_struct *entry;

	/* ex:Class rdf:type spindle:Class =>
	 *    Add class to match list if not present.
	 *
	 * ex:Class olo:index nnn =>
	 *    Add class to match list if not present; set score to nnn.
	 *
	 * ex:Class1 spindle:expressedAs ex:Class2 =>
	 *    Add ex:Class2 to match list if not present; add ex:Class1 as
	 *    one of the matched classes of ex:Class2.
	 */
	subject = librdf_statement_get_subject(statement);
	predicate = librdf_statement_get_predicate(statement);
	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_resource(subject) || !librdf_node_is_resource(predicate))
	{
		/* We're not interested in this triple */
		return 0;
	}
	subj = librdf_node_get_uri(subject);
	subjuri = (const char *) librdf_uri_as_string(subj);
	pred = librdf_node_get_uri(predicate);
	preduri = (const char *) librdf_uri_as_string(pred);
	if(!strcmp(preduri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"))
	{
		if(!librdf_node_is_resource(object))
		{
			return 0;
		}
		obj = librdf_node_get_uri(object);
		objuri = (const char *) librdf_uri_as_string(obj);
		if(strcmp(objuri, "http://bbcarchdev.github.io/ns/spindle#Class"))
		{
			return 0;
		}
		if(!(entry = spindle_class_add_(spindle, subjuri)))
		{
			return -1;
		}
		/* Check if there's an olo:index predicate attached to the class */
		s = librdf_new_node_from_node(subject);
		p = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) "http://purl.org/ontology/olo/core#index");
		query = librdf_new_statement_from_nodes(spindle->world, s, p, NULL);
		stream = librdf_model_find_statements(model, query);
		r = 0;
		while(!librdf_stream_end(stream))
		{
			st = librdf_stream_get_object(stream);
			r = spindle_class_set_score_(entry, st);
			if(r)
			{
				break;
			}
			librdf_stream_next(stream);
		}
		librdf_free_stream(stream);
		librdf_free_statement(query);
		if(r < 0)
		{
			return -1;
		}
		return 1;
	}
	if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#expressedAs"))
	{
		if(!librdf_node_is_resource(object))
		{
			return 0;
		}
		obj = librdf_node_get_uri(object);
		objuri = (const char *) librdf_uri_as_string(obj);
		if(!(entry = spindle_class_add_(spindle, objuri)))
		{
			return -1;
		}
		if(spindle_class_add_match_(entry, subjuri) < 0)
		{
			return -1;
		}
		return 1;
	}
	return 0;
}
