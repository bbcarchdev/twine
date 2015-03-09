/* Spindle: Co-reference aggregation engine
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

#include "p_spindle.h"

struct s3_upload_struct
{
	char *buf;
	size_t bufsize;
	size_t pos;
};

static int spindle_cache_init_(SPINDLECACHE *data, SPINDLE *spindle, const char *localname);
static int spindle_cache_cleanup_(SPINDLECACHE *data);
static int spindle_cache_store_(SPINDLECACHE *data);
static int spindle_cache_store_s3_(SPINDLECACHE *data);
static int spindle_cache_source_(SPINDLECACHE *data);
static int spindle_cache_source_sameas_(SPINDLECACHE *data);
static int spindle_cache_source_clean_(SPINDLECACHE *data);
static int spindle_cache_strset_refs_(SPINDLECACHE *data, struct spindle_strset_struct *set);
static size_t spindle_cache_s3_read_(char *buffer, size_t size, size_t nitems, void *userdata);
static int spindle_cache_describedby_(SPINDLECACHE *data);
static int spindle_cache_extra_(SPINDLECACHE *data);

/* Re-build the cached data for a set of proxies */
int
spindle_cache_update_set(SPINDLE *spindle, struct spindle_strset_struct *set)
{ 
	size_t c, origcount;

	/* Keep track of how many things were in the original set, so that we
	 * don't recursively re-cache a huge amount
	 */
	origcount = set->count;
	for(c = 0; c < set->count; c++)
	{
		if(c < origcount && (set->flags[c] & SF_MOVED))
		{
			spindle_cache_update(spindle, set->strings[c], set);
		}
		else
		{
			spindle_cache_update(spindle, set->strings[c], NULL);
		}
	}
	return 0;
}

/* Re-build the cached data for the proxy entity identified by localname;
 * if no references exist any more, the cached data will be removed.
 */
int
spindle_cache_update(SPINDLE *spindle, const char *localname, struct spindle_strset_struct *set)
{
	SPINDLECACHE data;

	if(spindle_cache_init_(&data, spindle, localname))
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": updating cache for <%s>\n", localname);
	/* Obtain cached source data */
	if(spindle_cache_source_(&data))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain cached data from store\n");
		spindle_cache_cleanup_(&data);
		return -1;
	}
	if(spindle_cache_strset_refs_(&data, set))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to process inbound references in source data\n");
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Update proxy classes */
	if(spindle_class_update(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Update proxy properties */
	if(spindle_prop_update(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Fetch information about the documents describing the entities */
	if(spindle_cache_describedby_(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Describe the document itself */
	if(spindle_doc_apply(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Describing licensing information */
	if(spindle_license_apply(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Store the resulting model */
	if(spindle_cache_store_(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Fetch data about related resources */
	if(spindle_cache_extra_(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	/* Store consolidated graphs in an S3 bucket */
	if(spindle_cache_store_s3_(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": cache update complete for <%s>\n", localname);
	spindle_cache_cleanup_(&data);
	return 0;
}

/* Initialise a data structure used to hold state while an individual proxy
 * entity is updated to reflect modified source data.
 */
static int
spindle_cache_init_(SPINDLECACHE *data, SPINDLE *spindle, const char *localname)
{	
	char *t;

	memset(data, 0, sizeof(SPINDLECACHE));
	data->spindle = spindle;
	data->sparql = spindle->sparql;
	data->localname = localname;
	data->score = 50;
	data->sameas = spindle->sameas;
	
	/* Create a node representing the full URI of the proxy entity */
	data->self = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) localname);
	if(!data->self)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for <%s>\n", localname);
		return -1;
	}
	/* Create a node representing the full URI of the proxy resource */
	data->docname = strdup(localname);		
	t = strchr(data->docname, '#');
	if(t)
	{
		*t = 0;
	}
	data->doc = librdf_new_node_from_uri_string(spindle->world, (unsigned const char *) data->docname);
	/* Set data->graphname to the URI of the named graph which will contain the proxy data, and
	 * data->graph to the corresponding node
	 */
	if(spindle->multigraph)
	{
		data->graphname = strdup(data->docname);
		data->graph = data->doc;
	}
	else
	{
		data->graphname = strdup(spindle->root);
		data->graph = spindle->rootgraph;
	}

	/* The rootdata model holds proxy data which is stored in the root
	 * graph for convenience
	 */
	if(!(data->rootdata = twine_rdf_model_create()))
	{
		return -1;
	}
	/* The sourcedata model holds data from the external data sources */
	if(!(data->sourcedata = twine_rdf_model_create()))
	{
		return -1;
	}
	/* The proxydata model holds the contents of the proxy graph */
	if(!(data->proxydata = twine_rdf_model_create()))
	{
		return -1;
	}
	/* The extradata model holds graphs which are related to this subject,
	 * and are fetched and cached in an S3 bucket if available.
	 */
	if(!(data->extradata = twine_rdf_model_create()))
	{
		return -1;
	}
	return 0;
}

/* Clean up the proxy entity update state data structure */
static int
spindle_cache_cleanup_(SPINDLECACHE *data)
{
	if(data->doc)
	{
		librdf_free_node(data->doc);
	}
	if(data->self)
	{
		librdf_free_node(data->self);
	}
	if(data->rootdata)
	{
		twine_rdf_model_destroy(data->rootdata);
	}
	if(data->proxydata)
	{
		twine_rdf_model_destroy(data->proxydata);
	}
	if(data->sourcedata)
	{
		twine_rdf_model_destroy(data->sourcedata);
	}
	if(data->extradata)
	{
		twine_rdf_model_destroy(data->extradata);
	}
	/* Never free data->graph - it is a pointer to data->doc or spindle->rootgraph */
	free(data->title);
	free(data->title_en);
	free(data->docname);
	free(data->graphname);
	return 0;
}

/* Obtain cached source data for processing */
static int
spindle_cache_source_(SPINDLECACHE *data)
{
	/* Find all of the triples related to all of the subjects linked to the
	 * proxy.
	 *
	 * Note that this includes data in both the root and proxy graphs,
	 * but they will be removed by spindle_cache_source_clean_().
	 */
	if(sparql_queryf_model(data->spindle->sparql, data->sourcedata,
						   "SELECT DISTINCT ?s ?p ?o ?g\n"
						   " WHERE {\n"
						   "  GRAPH %V {\n"
						   "   ?s %V %V .\n"
						   "  }\n"
						   "  GRAPH ?g {\n"
						   "   ?s ?p ?o .\n"
						   "  }\n"
						   "}",
						   data->spindle->rootgraph, data->sameas, data->self))
	{
		return -1;
	}
	if(spindle_cache_source_sameas_(data))
	{
		return -1;
	}
	if(spindle_cache_source_clean_(data))
	{
		return -1;
	}
	return 0;
}

/* Copy any owl:sameAs references into the proxy graph from the root
 * graph.
 */
static int
spindle_cache_source_sameas_(SPINDLECACHE *data)
{
	librdf_statement *query, *st;
	librdf_node *node;
	librdf_stream *stream;
	
	query = twine_rdf_st_create();
	if(!query)
	{
		return -1;
	}
	node = twine_rdf_node_clone(data->sameas);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	node = twine_rdf_node_clone(data->self);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_object(query, node);
	/* Create a stream querying for (?s owl:sameAs <self>) in the root graph
	 * from the source data
	 */
	stream = librdf_model_find_statements_with_options(data->sourcedata, query, data->spindle->rootgraph, NULL);
	if(!stream)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query model\n");
		twine_rdf_st_destroy(query);
		return -1;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		/* Add the statement to the proxy graph */
		if(twine_rdf_model_add_st(data->proxydata, st, data->graph))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to proxy model\n");
			librdf_free_stream(stream);
			twine_rdf_st_destroy(query);
			return -1;
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);	
	return 0;
}

/* Remove any proxy data from the source model */
static int
spindle_cache_source_clean_(SPINDLECACHE *data)
{
	librdf_iterator *iterator;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;

	iterator = librdf_model_get_contexts(data->sourcedata);
	while(!librdf_iterator_end(iterator))
	{
		node = librdf_iterator_get_object(iterator);
		uri = librdf_node_get_uri(node);
		uristr = (const char *) librdf_uri_as_string(uri);
		if(!strncmp(uristr, data->spindle->root, strlen(data->spindle->root)))
		{
			librdf_model_context_remove_statements(data->sourcedata, node);
		}
		librdf_iterator_next(iterator);
	}
	librdf_free_iterator(iterator);
	return 0;
}

/* Cache information about the digital objects describing the entity */
static int
spindle_cache_describedby_(SPINDLECACHE *data)
{
	librdf_iterator *iter;
	librdf_stream *stream;
	librdf_node *node, *subject;
	librdf_statement *st, *statement;
	const char *uri;

	/* Find all of the triples related to all of the graphs describing
	 * source data.
	 */
	iter = librdf_model_get_contexts(data->sourcedata);
	while(!librdf_iterator_end(iter))
	{
		node = librdf_iterator_get_object(iter);
		uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(node));
		if(!strncmp(uri, data->spindle->root, strlen(data->spindle->root)))
		{
			librdf_iterator_next(iter);
			continue;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": fetching information about graph <%s>\n", uri);
		/* Fetch triples from graph G where the subject of each
		 * triple is also graph G
		 */
		if(spindle_graph_description_node(data->spindle, data->sourcedata, node))
		{
			return -1;
		}
		/* Add triples, in our graph, stating that:
		 *   ex:graphuri rdf:type foaf:Document .
		 */
		st = twine_rdf_st_create();
		librdf_statement_set_subject(st, librdf_new_node_from_node(node));
		librdf_statement_set_predicate(st, twine_rdf_node_createuri(NS_RDF "type"));
		librdf_statement_set_object(st, twine_rdf_node_createuri(NS_FOAF "Document"));
		twine_rdf_model_add_st(data->proxydata, st, data->graph);
		librdf_free_statement(st);
		
		/* For each subject in the graph, add triples stating that:
		 *   ex:subject wdrs:describedBy ex:graphuri .
		 */
		stream = librdf_model_context_as_stream(data->sourcedata, node);
		for(; !librdf_stream_end(stream); librdf_stream_next(stream))
		{
			statement = librdf_stream_get_object(stream);
			subject = librdf_statement_get_subject(statement);
			if(!librdf_node_is_resource(subject))
			{
				continue;
			}
			if(librdf_node_equals(node, subject))
			{
				continue;
			}
			
			st = twine_rdf_st_create();
			librdf_statement_set_subject(st, librdf_new_node_from_node(subject));
			librdf_statement_set_predicate(st, twine_rdf_node_createuri(NS_POWDER "describedBy"));
			librdf_statement_set_object(st, librdf_new_node_from_node(node));
			twine_rdf_model_add_st(data->proxydata, st, data->graph);
			librdf_free_statement(st);

			/* Add <doc> rdfs:seeAlso <source> */
			st = twine_rdf_st_create();
			librdf_statement_set_subject(st, librdf_new_node_from_node(data->doc));
			librdf_statement_set_predicate(st, twine_rdf_node_createuri(NS_RDFS "seeAlso"));
			librdf_statement_set_object(st, librdf_new_node_from_node(node));
			twine_rdf_model_add_st(data->proxydata, st, data->graph);
			librdf_free_statement(st);
		}
		librdf_free_stream(stream);

		librdf_iterator_next(iter);
	}
	librdf_free_iterator(iter);
	return 0;
}

static int
spindle_cache_extra_(SPINDLECACHE *data)
{
	librdf_iterator *iterator;
	librdf_node *context;
	librdf_uri *uri;
	const char *uristr;

	/* If there's no S3 bucket, the extradata model won't be used, so
	 * there's nothing to do here
	 */
	if(!data->spindle->bucket)
	{
		return 0;
	}
	/* Cache information about external resources related to this
	 * entity, restricted by the predicate used for the relation
	 */
	if(sparql_queryf_model(data->spindle->sparql, data->extradata,
						   "SELECT DISTINCT ?s ?p ?o ?g\n"
						   " WHERE {\n"
						   "  GRAPH %V {\n"
						   "   %V ?p1 ?s .\n"
						   "   FILTER("
						   "     ?p1 = <" NS_FOAF "page> || "
						   "     ?p1 = <" NS_MRSS "player> "
						   "   )\n"
						   "  }\n"
						   "  GRAPH ?g {\n"
						   "   ?s ?p ?o .\n"
						   "  }\n"
						   "  FILTER(?g != %V && ?g != %V)\n"
						   "}",
						   data->graph, data->self, data->graph, data->spindle->rootgraph))
	{
		return -1;
	}
	/* Remove anything in a local graph */
	iterator = librdf_model_get_contexts(data->extradata);
	while(!librdf_iterator_end(iterator))
	{
		context = librdf_iterator_get_object(iterator);
		uri = librdf_node_get_uri(context);
		uristr = (const char *) librdf_uri_as_string(uri);
		if(!strncmp(uristr, data->spindle->root, strlen(data->spindle->root)))
		{
			librdf_model_context_remove_statements(data->extradata, context);
		}
		librdf_iterator_next(iterator);
	}
	librdf_free_iterator(iterator);
	return 0;
}

/* Write changes to a proxy entity back to the store */
static int
spindle_cache_store_(SPINDLECACHE *data)
{
	char *triples;
	size_t triplen;

	/* First update the root graph */

	/* Note that our owl:sameAs statements take the form
	 * <external> owl:sameAs <proxy>, so we can delete <proxy> ?p ?o with
	 * impunity.
	 */
	if(sparql_updatef(data->spindle->sparql,
					  "WITH %V\n"
					  " DELETE { %V ?p ?o }\n"
					  " WHERE { %V ?p ?o }",
					  data->spindle->rootgraph, data->self, data->self))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
		return -1;
	}
	if(sparql_updatef(data->spindle->sparql,
					  "WITH %V\n"
					  " DELETE { %V ?p ?o }\n"
					  " WHERE { %V ?p ?o }",
					  data->spindle->rootgraph, data->doc, data->doc))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
		return -1;
	}
	if(sparql_insert_model(data->spindle->sparql, data->rootdata))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to push new proxy data into the root graph of the store\n");
		return -1;
	}

	/* Now update the proxy data */
	if(data->spindle->multigraph)
	{
		triples = twine_rdf_model_ntriples(data->proxydata, &triplen);
		if(sparql_put(data->spindle->sparql, data->graphname, triples, triplen))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to push new proxy data into the store\n");
			librdf_free_memory(triples);
			return -1;
		}
		librdf_free_memory(triples);
	}
	else
	{
		if(sparql_updatef(data->spindle->sparql,
						  "WITH %V\n"
						  " DELETE { %V ?p ?o }\n"
						  " WHERE { %V ?p ?o }",
						  data->graph, data->self, data->self))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
			return -1;
		}
		/* Insert the new proxy triples, if any */
		if(sparql_insert_model(data->spindle->sparql, data->proxydata))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to push new proxy data into the store\n");
			return -1;
		}
	}
	return 0;
}

static int
spindle_cache_store_s3_(SPINDLECACHE *data)
{
	char *proxy, *source, *extra, *urlbuf, *t;
	size_t proxylen, sourcelen, extralen, l;
	char nqlenstr[256];
	S3REQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;

	if(!data->spindle->bucket)
	{
		return 0;
	}
	if(data->spindle->multigraph)
	{
		/* Remove the root graph from the proxy data model, if it's present */
		librdf_model_context_remove_statements(data->proxydata, data->spindle->rootgraph);
	}
	proxy = twine_rdf_model_nquads(data->proxydata, &proxylen);
	if(!proxy)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise proxy model as N-Quads\n");
		return -1;
	}
	source = twine_rdf_model_nquads(data->sourcedata, &sourcelen);
	if(!source)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise source model as N-Quads\n");
		librdf_free_memory(proxy);
		return -1;
	}

	extra = twine_rdf_model_nquads(data->extradata, &extralen);
	if(!extra)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise extra model as N-Quads\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
		return -1;
	}
	memset(&s3data, 0, sizeof(struct s3_upload_struct));
	s3data.bufsize = proxylen + sourcelen + extralen + 3 + 128;
	s3data.buf = (char *) calloc(1, s3data.bufsize + 1);
	if(!s3data.buf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for consolidated N-Quads\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
		librdf_free_memory(extra);
		return -1;
	}
	strcpy(s3data.buf, "## Proxy:\n");
	l = strlen(s3data.buf);

	if(proxylen)
	{
		memcpy(&(s3data.buf[l]), proxy, proxylen);
	}
	strcpy(&(s3data.buf[l + proxylen]), "\n## Source:\n");
	l = strlen(s3data.buf);

	if(sourcelen)
	{
		memcpy(&(s3data.buf[l]), source, sourcelen);
	}
	strcpy(&(s3data.buf[l + sourcelen]), "\n## Extra:\n");
	l = strlen(s3data.buf);
	
	if(extralen)
	{
		memcpy(&(s3data.buf[l]), extra, extralen);
	}
	strcpy(&(s3data.buf[l + extralen]), "\n## End\n");
	s3data.bufsize = strlen(s3data.buf);

	librdf_free_memory(proxy);
	librdf_free_memory(source);
	librdf_free_memory(extra);
	urlbuf = (char *) malloc(1 + strlen(data->localname) + 4 + 1);
	if(!urlbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for URL\n");
		return -1;
	}
	urlbuf[0] = '/';
	if((t = strrchr(data->localname, '/')))
	{
		t++;
	}
	else
	{
		t = (char *) data->localname;
	}
	strcpy(&(urlbuf[1]), t);
	if((t = strchr(urlbuf, '#')))
	{
		*t = 0;
	}
	req = s3_request_create(data->spindle->bucket, urlbuf, "PUT");
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, data->spindle->s3_verbose);
	curl_easy_setopt(ch, CURLOPT_READFUNCTION, spindle_cache_s3_read_);
	curl_easy_setopt(ch, CURLOPT_READDATA, &s3data);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE, (long) s3data.bufsize);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
	headers = curl_slist_append(s3_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Content-Type: application/nquads");
	headers = curl_slist_append(headers, "x-amz-acl: public-read");
	sprintf(nqlenstr, "Content-Length: %u", (unsigned) s3data.bufsize);
	headers = curl_slist_append(headers, nqlenstr);
	s3_request_set_headers(req, headers);
	r = 0;
	if((e = s3_request_perform(req)))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to upload N-Quads to bucket at <%s>: %s\n", urlbuf, curl_easy_strerror(e));
		r = -1;
	}
	else
	{
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status != 200)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to upload N-Quads to bucket at <%s> (HTTP status %ld)\n", urlbuf, status);
			r = -1;
		}
	}
	s3_request_destroy(req);
	free(urlbuf);
	free(s3data.buf);
	return r;
}

static size_t
spindle_cache_s3_read_(char *buffer, size_t size, size_t nitems, void *userdata)
{
	struct s3_upload_struct *data;

	data = (struct s3_upload_struct *) userdata;
	size *= nitems;
	if(size > data->bufsize - data->pos)
	{
		size = data->bufsize - data->pos;
	}
	memcpy(buffer, &(data->buf[data->pos]), size);
	data->pos += size;
	return size;
}

/* For anything which is the subject of one of the triples in the source
 * dataset, find any triples whose object is that thing and add the
 * subjects to the set if they're not already present.
 */
static int
spindle_cache_strset_refs_(SPINDLECACHE *data, struct spindle_strset_struct *set)
{
	struct spindle_strset_struct *subjects;
	librdf_stream *stream;
	librdf_statement *st;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	size_t c;
	SPARQLRES *res;
	SPARQLROW *row;

	if(!set)
	{
		return 0;
	}
	/* Build the subject set */
	subjects = spindle_strset_create();
	stream = librdf_model_as_stream(data->sourcedata);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_subject(st);
		if(librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			spindle_strset_add(subjects, uristr);
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
		
	/* Query for references to each of the subjects */
	for(c = 0; c < subjects->count; c++)
	{
		res = sparql_queryf(data->spindle->sparql,
							"SELECT ?local, ?s WHERE {\n"
							" GRAPH %V {\n"
							"  ?s <" NS_OWL "sameAs> ?local .\n"
							" }\n"
							" GRAPH ?g {\n"
							"   ?s ?p <%s> .\n"
							" }\n"
							"}",
							data->spindle->rootgraph, subjects->strings[c]);
		if(!res)
		{
			twine_logf(LOG_ERR, "SPARQL query for inbound references failed\n");
			spindle_strset_destroy(subjects);
			return -1;
		}
		/* Add each result to the changeset */
		while((row = sparqlres_next(res)))
		{
			node = sparqlrow_binding(row, 0);
			if(node && librdf_node_is_resource(node) &&
			   (uri = librdf_node_get_uri(node)) &&
			   (uristr = (const char *) librdf_uri_as_string(uri)))
			{
				spindle_strset_add(set, uristr);
			}				
		}
		sparqlres_destroy(res);
	}
	spindle_strset_destroy(subjects);
	return 0;
}

