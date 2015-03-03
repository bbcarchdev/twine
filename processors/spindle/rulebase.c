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
static int spindle_rulebase_add_statement_(SPINDLE *spindle, librdf_model *model, librdf_statement *statement);

static int spindle_class_add_node_(SPINDLE *spindle, librdf_model *model, const char *uri, librdf_node *node);
static int spindle_class_compare_(const void *ptra, const void *ptrb);
static struct spindle_classmatch_struct *spindle_class_add_(SPINDLE *spindle, const char *uri);
static int spindle_class_add_match_(struct spindle_classmatch_struct *match, const char *uri);
static int spindle_class_set_score_(struct spindle_classmatch_struct *entry, librdf_statement *statement);
static int spindle_class_dump_(SPINDLE *spindle);

static int spindle_pred_add_node_(SPINDLE *spindle, librdf_model *model, const char *uri, librdf_node *node);
static int spindle_pred_add_matchnode_(SPINDLE *spindle, librdf_model *model, const char *preduri, librdf_node *matchnode);
static int spindle_pred_compare_(const void *ptra, const void *ptrb);
static struct spindle_predicatemap_struct *spindle_pred_add_(SPINDLE *spindle, const char *preduri);
static int spindle_pred_add_match_(struct spindle_predicatemap_struct *map, const char *matchuri, const char *classuri, int score);
static int spindle_pred_set_score_(struct spindle_predicatemap_struct *map, librdf_statement *statement);
static int spindle_pred_set_expect_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_pred_set_expecttype_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_pred_set_proxyonly_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_pred_set_indexed_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_pred_dump_(SPINDLE *spindle);

static int spindle_cachepred_add_(SPINDLE *spindle, const char *uri);
static int spindle_cachepred_dump_(SPINDLE *spindle);
static int spindle_cachepred_compare_(const void *ptra, const void *ptrb);

static int spindle_coref_add_matchnode_(SPINDLE *spindle, const char *predicate, librdf_node *node);
static int spindle_coref_add_(SPINDLE *spindle, const char *predicate, struct coref_match_struct *match);

static struct coref_match_struct coref_match_types[] = 
{
	{ "http://bbcarchdev.github.io/ns/spindle#resourceMatch", spindle_match_sameas },
	{ "http://bbcarchdev.github.io/ns/spindle#wikipediaMatch", spindle_match_wikipedia },
	{ NULL, NULL }
};

int
spindle_rulebase_init(SPINDLE *spindle)
{
	char *rulebase, *buf;
	librdf_model *model;
	librdf_stream *stream;
	librdf_statement *statement;

	spindle_cachepred_add_(spindle, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	spindle_cachepred_add_(spindle, "http://www.w3.org/2002/07/owl#sameAs");
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
		spindle_rulebase_add_statement_(spindle, model, statement);
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_model(model);
	qsort(spindle->classes, spindle->classcount, sizeof(struct spindle_classmatch_struct), spindle_class_compare_);
	qsort(spindle->predicates, spindle->predcount, sizeof(struct spindle_predicatemap_struct), spindle_pred_compare_);
	qsort(spindle->cachepreds, spindle->cpcount, sizeof(char *), spindle_cachepred_compare_);
	spindle_class_dump_(spindle);
	spindle_pred_dump_(spindle);
	spindle_cachepred_dump_(spindle);
	return 0;
}

static int
spindle_class_dump_(SPINDLE *spindle)
{
	size_t c, d;

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

static int
spindle_pred_dump_(SPINDLE *spindle)
{
	size_t c, d;
	const char *expect, *po;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": predicates rule-base (%d entries):\n", (int) spindle->predcount);
	for(c = 0; c < spindle->predcount; c++)
	{
		switch(spindle->predicates[c].expected)
		{
		case RAPTOR_TERM_TYPE_URI:
			expect = "URI";
			break;
		case RAPTOR_TERM_TYPE_LITERAL:
			expect = "literal";
			break;
		default:
			expect = "unknown";
		}
		if(spindle->predicates[c].proxyonly)
		{
			po = " [proxy-only]";
		}
		else
		{
			po = "";
		}
		if(spindle->predicates[c].datatype)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s> (%s <%s>) %s\n", spindle->predicates[c].score, spindle->predicates[c].target, expect, spindle->predicates[c].datatype, po);
		}
		else
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s> (%s) %s\n", spindle->predicates[c].score, spindle->predicates[c].target, expect, po);
		}
		for(d = 0; d < spindle->predicates[c].matchcount; d++)
		{
			if(spindle->predicates[c].matches[d].onlyfor)
			{
				twine_logf(LOG_DEBUG, PLUGIN_NAME "  +--> %d: <%s> (for <%s>)\n",
						   spindle->predicates[c].matches[d].priority,
						   spindle->predicates[c].matches[d].predicate,
						   spindle->predicates[c].matches[d].onlyfor);
			}
			else
			{
				twine_logf(LOG_DEBUG, PLUGIN_NAME "  +--> %d: <%s>\n",
						   spindle->predicates[c].matches[d].priority,
						   spindle->predicates[c].matches[d].predicate);
			}
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

static int
spindle_pred_compare_(const void *ptra, const void *ptrb)
{
	struct spindle_predicatemap_struct *a, *b;
	
	a = (struct spindle_predicatemap_struct *) ptra;
	b = (struct spindle_predicatemap_struct *) ptrb;
	
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
	spindle_class_add_match_(p, uri);
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
spindle_rulebase_add_statement_(SPINDLE *spindle, librdf_model *model, librdf_statement *statement)
{
	librdf_node *subject, *predicate, *object;
	librdf_uri *subj, *pred, *obj;
	const char *subjuri, *preduri, *objuri;
	struct spindle_classmatch_struct *classentry;

	/* ex:Class rdf:type spindle:Class =>
	 *    Add class to match list if not present.
	 *
	 *  -- ex:Class olo:index nnn =>
	 *     set score to nnn.
	 *
	 * ex:Class1 spindle:expressedAs ex:Class2 =>
	 *    Add ex:Class2 to match list if not present; add ex:Class1 as
	 *    one of the matched classes of ex:Class2.
	 *
	 * ex:prop rdf:type spindle:Property =>
	 *    Add predicate to mapping list if not present
	 *
	 *  -- ex:prop spindle:expect spindle:literal, spindle:uri =>
	 *     set predicate mapping 'expected type'
	 *  -- ex:prop spindle:expectDataType =>
	 *     set predicate mapping 'expected datatype'
	 *  -- ex:prop spindle:proxyOnly =>
	 *     set predicate mapping 'proxy-only' flag
	 *
	 * ex:prop spindle:property _:bnode =>
	 *    Process _:bnode as a predicate-match entry for the predicate ex:prop
	 *    (The bnode itself will specify the predicate mapping to attach
	 *    the match entry to).
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
	/* ex:Class a spindle:Class
	 * ex:predicate a spindle:Property
	 */
	if(!strcmp(preduri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"))
	{
		if(!librdf_node_is_resource(object))
		{
			return 0;
		}
		obj = librdf_node_get_uri(object);
		objuri = (const char *) librdf_uri_as_string(obj);
		if(!strcmp(objuri, "http://bbcarchdev.github.io/ns/spindle#Class"))
		{
			if(spindle_class_add_node_(spindle, model, subjuri, subject) < 0)
			{
				return -1;
			}
			return 1;
		}
		if(!strcmp(objuri, "http://bbcarchdev.github.io/ns/spindle#Property"))
		{
			if(spindle_pred_add_node_(spindle, model, subjuri, subject) < 0)
			{
				return -1;
			}
			return 1;
		}
		return 0;
	}
	/* ex:Class spindle:expressedAs ex:OtherClass */
	if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#expressedAs"))
	{
		if(!librdf_node_is_resource(object))
		{
			return 0;
		}
		obj = librdf_node_get_uri(object);
		objuri = (const char *) librdf_uri_as_string(obj);
		if(!(classentry = spindle_class_add_(spindle, objuri)))
		{
			return -1;
		}
		if(spindle_class_add_match_(classentry, subjuri) < 0)
		{
			return -1;
		}
		return 1;
	}
	/* ex:predicate spindle:property [ ... ] */
	if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#property"))
	{
		if(librdf_node_is_literal(object))
		{
			return 0;
		}
		return spindle_pred_add_matchnode_(spindle, model, subjuri, object);
	}
	/* ex:predicate spindle:coref spindle:foo */
	if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#coref"))
	{
		return spindle_coref_add_matchnode_(spindle, subjuri, object);
	}
	return 0;
}

/* Add a spindle:coref statement to the coreference matching ruleset */
static int
spindle_coref_add_matchnode_(SPINDLE *spindle, const char *predicate, librdf_node *node)
{
	librdf_uri *uri;
	const char *uristr;
	size_t c;

	if(!librdf_node_is_resource(node))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": spindle:coref statement expected a resource object\n");
		return 0;
	}
	uri = librdf_node_get_uri(node);
	uristr = (const char *) librdf_uri_as_string(uri);
	for(c = 0; coref_match_types[c].predicate; c++)
	{
		if(!strcmp(uristr, coref_match_types[c].predicate))
		{
			spindle_cachepred_add_(spindle, predicate);
			return spindle_coref_add_(spindle, predicate, &(coref_match_types[c]));
		}
	}
	twine_logf(LOG_ERR, PLUGIN_NAME ": co-reference match type <%s> is not supported\n", uristr);
	return 0;
}

static int
spindle_coref_add_(SPINDLE *spindle, const char *predicate, struct coref_match_struct *match)
{
	struct coref_match_struct *p;
	size_t c;

	for(c = 0; c < spindle->corefcount; c++)
	{
		if(!strcmp(predicate, spindle->coref[c].predicate))
		{
			spindle->coref[c].callback = match->callback;
			return 0;
		}
	}
	if(spindle->corefcount + 1 > spindle->corefsize)
	{
		p = (struct coref_match_struct *) realloc(spindle->coref, sizeof(struct coref_match_struct) * (spindle->corefsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to resize co-reference match type list\n");
			return -1;
		}
		spindle->coref = p;
		spindle->corefsize += 4;
	}
	p = &(spindle->coref[spindle->corefcount]);
	p->predicate = strdup(predicate);
	if(!p->predicate)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate co-reference predicate URI\n");
		return -1;
	}
	p->callback = match->callback;
	spindle->corefcount++;
	return 1;
}

/* Given an instance of a spindle:Class, add it to the rulebase */
static int
spindle_class_add_node_(SPINDLE *spindle, librdf_model *model, const char *uri, librdf_node *node)
{
	librdf_statement *query, *statement;
	librdf_node *s, *predicate;
	librdf_uri *pred;
	librdf_stream *stream;
	const char *preduri;
	int r;
	struct spindle_classmatch_struct *classentry;
	
	if(!(classentry = spindle_class_add_(spindle, uri)))
	{
		return -1;
	}
	/* Process the properties of the instance */
	s = librdf_new_node_from_node(node);
	query = librdf_new_statement_from_nodes(spindle->world, s, NULL, NULL);
	stream = librdf_model_find_statements(model, query);
	r = 0;
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		predicate = librdf_statement_get_predicate(statement);
		pred = librdf_node_get_uri(predicate);
		preduri = (const char *) librdf_uri_as_string(pred);
		if(!strcmp(preduri, "http://purl.org/ontology/olo/core#index"))
		{
			if((r = spindle_class_set_score_(classentry, statement)))
			{
				break;
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(r)
	{
		return -1;
	}
	return 1;
}


/* Given an instance of a spindle:Property, add it to the rulebase */
static int
spindle_pred_add_node_(SPINDLE *spindle, librdf_model *model, const char *uri, librdf_node *node)
{
	librdf_statement *query, *statement;
	librdf_node *s, *predicate;
	librdf_uri *pred;
	librdf_stream *stream;
	const char *preduri;
	int r;
	struct spindle_predicatemap_struct *predentry;
	
	if(!(predentry = spindle_pred_add_(spindle, uri)))
	{
		return -1;
	}
	/* Process the properties of the instance */
	s = librdf_new_node_from_node(node);
	query = librdf_new_statement_from_nodes(spindle->world, s, NULL, NULL);
	stream = librdf_model_find_statements(model, query);
	r = 0;
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		predicate = librdf_statement_get_predicate(statement);
		pred = librdf_node_get_uri(predicate);
		preduri = (const char *) librdf_uri_as_string(pred);
		/* ex:predicate olo:index nnn */
		if(!strcmp(preduri, "http://purl.org/ontology/olo/core#index"))
		{
			r = spindle_pred_set_score_(predentry, statement);
		}
		/* ex:predicate spindle:expect rdfs:Literal
		 * ex:predicate spindle:expect rdfs:Resource
		 */
		if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#expect"))
		{
			r = spindle_pred_set_expect_(predentry, statement);
		}
		/* ex:predicate spindle:expectType xsd:decimal */
		if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#expectType"))
		{
			r = spindle_pred_set_expecttype_(predentry, statement);
		}
		/* ex:predicate spindle:proxyOnly "true"^^xsd:boolean */
		if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#proxyOnly"))
		{
			r = spindle_pred_set_proxyonly_(predentry, statement);
		}
		/* ex:predicate spindle:indexed "true"^^xsd:boolean */
		if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#indexed"))
		{
			r = spindle_pred_set_indexed_(predentry, statement);
		}
		if(r < 0)
		{
			break;
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(r)
	{
		return -1;
	}
	return 1;
}

static int
spindle_pred_set_score_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
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
spindle_pred_set_expect_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *obj;
	const char *objuri;
		
	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_resource(object))
	{
		return 0;
	}
	obj = librdf_node_get_uri(object);
	objuri = (const char *) librdf_uri_as_string(obj);
	if(!strcmp(objuri, "http://www.w3.org/2000/01/rdf-schema#Literal"))
	{
		entry->expected = RAPTOR_TERM_TYPE_LITERAL;
		return 1;
	}
	if(!strcmp(objuri, "http://www.w3.org/2000/01/rdf-schema#Resource"))
	{
		entry->expected = RAPTOR_TERM_TYPE_URI;
		return 1;
	}
	twine_logf(LOG_WARNING, PLUGIN_NAME ": unexpected spindle:expect value <%s> for <%s>\n", objuri, entry->target);
	return 0;
}

static int
spindle_pred_set_expecttype_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *obj;
	const char *objuri;
	char *str;

	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_resource(object))
	{
		return 0;
	}
	obj = librdf_node_get_uri(object);
	objuri = (const char *) librdf_uri_as_string(obj);
	str = strdup(objuri);
	if(!str)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate datatype URI\n");
		return -1;
	}
	if(entry->datatype)
	{
		free(entry->datatype);
	}
	entry->datatype = str;
	return 0;
}

static int
spindle_pred_set_proxyonly_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *dt;
	const char *dturi, *objstr;
	
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
	if(strcmp(dturi, "http://www.w3.org/2001/XMLSchema#boolean"))
	{
		return 0;
	}
	objstr = (const char *) librdf_node_get_literal_value(object);
	if(!strcmp(objstr, "true"))
	{
		entry->proxyonly = 1;
	}
	else
	{
		entry->proxyonly = 0;
	}
	return 1;
}

static int
spindle_pred_set_indexed_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *dt;
	const char *dturi, *objstr;
	
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
	if(strcmp(dturi, "http://www.w3.org/2001/XMLSchema#boolean"))
	{
		return 0;
	}
	objstr = (const char *) librdf_node_get_literal_value(object);
	if(!strcmp(objstr, "true"))
	{
		entry->indexed = 1;
	}
	else
	{
		entry->indexed = 0;
	}
	return 1;
}

static int
spindle_pred_add_matchnode_(SPINDLE *spindle, librdf_model *model, const char *matchuri, librdf_node *matchnode)
{
	librdf_node *s, *predicate, *p, *object;
	librdf_uri *pred, *obj, *dt;
	librdf_statement *query, *statement;
	librdf_stream *stream;
	const char *preduri, *objuri, *dturi;
	struct spindle_predicatemap_struct *entry;
	int r, hasdomain, score, i;

	/* Find triples whose subject is <matchnode>, which are expected to take
	 * the form:
	 *
	 * _:foo olo:index nnn ;
	 *   spindle:expressedAs ex:property1 ;
	 *   rdfs:domain ex:Class1, ex:Class2 ... .
	 */
	s = librdf_new_node_from_node(matchnode);
	query = librdf_new_statement_from_nodes(spindle->world, s, NULL, NULL);
	stream = librdf_model_find_statements(model, query);
	score = 100;
	r = 0;
	hasdomain = 0;
	entry = NULL;
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);		
		predicate = librdf_statement_get_predicate(statement);
		object = librdf_statement_get_object(statement);
		if(!librdf_node_is_resource(predicate))
		{
			librdf_stream_next(stream);
			continue;
		}
		pred = librdf_node_get_uri(predicate);
		preduri = (const char *) librdf_uri_as_string(pred);
		if(!strcmp(preduri, "http://www.w3.org/2000/01/rdf-schema#domain"))
		{
			hasdomain = 1;
		}
		else if(!strcmp(preduri, "http://purl.org/ontology/olo/core#index"))
		{
			if(librdf_node_is_literal(object) &&
			   (dt = librdf_node_get_literal_value_datatype_uri(object)))
			{
				dturi = (const char *) librdf_uri_as_string(dt);
				if(!strcmp(dturi, "http://www.w3.org/2001/XMLSchema#integer"))
				{
					i = atoi((const char *) librdf_node_get_literal_value(object));
					if(i >= 0)
					{
						score = i;
					}
				}
			}
		}
		else if(!strcmp(preduri, "http://bbcarchdev.github.io/ns/spindle#expressedAs"))
		{
			if(!librdf_node_is_resource(object))
			{
				librdf_stream_next(stream);
				continue;
			}
			obj = librdf_node_get_uri(object);
			objuri = (const char *) librdf_uri_as_string(obj);
			if(!(entry = spindle_pred_add_(spindle, objuri)))
			{
				r = -1;
				break;
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(r < 0)
	{
		return -1;
	}
	if(!entry)
	{
		return 0;
	}
	if(spindle_cachepred_add_(spindle, matchuri))
	{
		return -1;
	}
	if(!hasdomain)
	{
		/* This isn't a domain-specific mapping; just add the target
		 * predicate <matchuri> to entry's match list.
		 */
		spindle_pred_add_match_(entry, matchuri, NULL, score);
		return 0;
	}
	/* For each rdfs:domain, add the target predicate <matchuri> along with
	 * the listed domain to the entry's match list.
	 */
	s = librdf_new_node_from_node(matchnode);
	p = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) "http://www.w3.org/2000/01/rdf-schema#domain");
	query = librdf_new_statement_from_nodes(spindle->world, s, p, NULL);
	stream = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		object = librdf_statement_get_object(statement);
		if(!librdf_node_is_resource(object))
		{
			librdf_stream_next(stream);
			continue;
		}
		obj = librdf_node_get_uri(object);
		objuri = (const char *) librdf_uri_as_string(obj);
		spindle_pred_add_match_(entry, matchuri, objuri, score);	
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(statement);
	return 0;
}

static struct spindle_predicatemap_struct *
spindle_pred_add_(SPINDLE *spindle, const char *preduri)
{
	size_t c;
	struct spindle_predicatemap_struct *p;

	if(spindle_cachepred_add_(spindle, preduri))
	{
		return NULL;
	}
	for(c = 0; c < spindle->predcount; c++)
	{
		if(!strcmp(spindle->predicates[c].target, preduri))
		{
			return &(spindle->predicates[c]);
		}
	}
	if(spindle->predcount + 1 >= spindle->predsize)
	{
		p = (struct spindle_predicatemap_struct *) realloc(spindle->predicates, sizeof(struct spindle_predicatemap_struct) * (spindle->predsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand predicate map list\n");
			return NULL;
		}
		spindle->predicates = p;
		memset(&(p[spindle->predsize]), 0, sizeof(struct spindle_predicatemap_struct) * (4 + 1));
		spindle->predsize += 4;
	}
	p = &(spindle->predicates[spindle->predcount]);
	p->target = strdup(preduri);
	p->expected = RAPTOR_TERM_TYPE_UNKNOWN;
	p->proxyonly = 0;
	p->score = 100;
	if(!p->target)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate predicate URI\n");
		return NULL;
	}
	spindle->predcount++;
	return p;
}

static int
spindle_pred_add_match_(struct spindle_predicatemap_struct *map, const char *matchuri, const char *classuri, int score)
{
	size_t c;
	struct spindle_predicatematch_struct *p;

	for(c = 0; c < map->matchcount; c++)
	{
		if(strcmp(map->matches[c].predicate, matchuri))
		{
			continue;
		}
		if((classuri && !map->matches[c].onlyfor) ||
		   (!classuri && map->matches[c].onlyfor))
		{
			continue;
		}
		if(classuri && strcmp(map->matches[c].onlyfor, classuri))
		{
			continue;
		}
		map->matches[c].priority = score;
		return 1;
	}
	if(map->matchcount + 1 >= map->matchsize)
	{
		p = (struct spindle_predicatematch_struct *) realloc(map->matches, sizeof(struct spindle_predicatematch_struct) * (map->matchsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to resize predicate match list\n");
			return -1;
		}
		map->matches = p;
		memset(&(p[map->matchsize]), 0, sizeof(struct spindle_predicatematch_struct) * (4 + 1));
		map->matchsize += 4;
	}
	p = &(map->matches[map->matchcount]);
	p->predicate = strdup(matchuri);
	if(!p->predicate)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate predicate match URI\n");
		return -1;
	}
	if(classuri)
	{
		p->onlyfor = strdup(classuri);
		if(!p->onlyfor)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate predicate match class URI\n");
			free(p->predicate);
			p->predicate = NULL;
			return -1;
		}
	}
	p->priority = score;
	map->matchcount++;
	return 1;
}

static int
spindle_cachepred_add_(SPINDLE *spindle, const char *uri)
{
	size_t c;
	char **p;

	for(c = 0; c < spindle->cpcount; c++)
	{
		if(!strcmp(uri, spindle->cachepreds[c]))
		{
			return 0;
		}
	}
	if(spindle->cpcount + 1 >= spindle->cpsize)
	{
		p = (char **) realloc(spindle->cachepreds, sizeof(char *) * (spindle->cpsize + 8 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand cached predicates list\n");
			return -1;
		}
		spindle->cachepreds = p;
		memset(&(p[spindle->cpcount]), 0, sizeof(char *) * (8 + 1));
		spindle->cpsize += 8;
	}
	p = &(spindle->cachepreds[spindle->cpcount]);
	p[0] = strdup(uri);
	if(!p[0])
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate cached predicate URI\n");
		return -1;
	}
	spindle->cpcount++;
	return 0;
}

static int
spindle_cachepred_dump_(SPINDLE *spindle)
{
	size_t c;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": cached predicates set (%d entries):\n", (int) spindle->cpcount);
	for(c = 0; c < spindle->cpcount; c++)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s>\n", (int) c, spindle->cachepreds[c]);
	}
	return 0;
}

static int
spindle_cachepred_compare_(const void *ptra, const void *ptrb)
{
	char **a, **b;
	
	a = (char **) ptra;
	b = (char **) ptrb;
	
	return strcmp(*a, *b);
}
