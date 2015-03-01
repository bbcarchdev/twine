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
	twine_logf(LOG_INFO, PLUGIN_NAME ": updating <%s>\n", localname);
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
	/* Store the resulting model */
	if(spindle_cache_store_(&data) < 0)
	{
		spindle_cache_cleanup_(&data);
		return -1;
	}
	spindle_cache_cleanup_(&data);
	return 0;
}

/* Obtain cached source data for processing */
static int
spindle_cache_source_(SPINDLECACHE *data)
{
	/* Find all of the triples related to all of the subjects linked to the
	 * proxy.
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

/* Copy any of our own owl:sameAs references into the proxy graph */
static int
spindle_cache_source_sameas_(SPINDLECACHE *data)
{
	librdf_statement *query, *st;
	librdf_node *node;
	librdf_stream *stream, *qstream;
	
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
	/* Create a stream querying for (?s owl:sameAs <self>) in the root graph */
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
		/* Check if the owl:sameAs statement is already present in the proxy
		 * data graph
		 */
		qstream = librdf_model_find_statements_with_options(data->proxydata, st, data->graph, NULL);
		if(!qstream)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query model\n");
			librdf_free_stream(stream);
			twine_rdf_st_destroy(query);
			return -1;
		}
		if(!librdf_stream_end(qstream))
		{
			/* If so, skip to the next item */
			librdf_free_stream(qstream);
			librdf_stream_next(stream);
			continue;
		}
		librdf_free_stream(qstream);
		/* Add the owl:sameAs statement to the proxy data graph */
		if(librdf_model_context_add_statement(data->proxydata, data->graph, st))
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
	librdf_model_context_remove_statements(data->sourcedata, data->graph);
	if(data->graph != data->spindle->rootgraph)
	{
		librdf_model_context_remove_statements(data->sourcedata, data->spindle->rootgraph);
	}
	return 0;
}

/* Write changes to a proxy entity back to the store */
static int
spindle_cache_store_(SPINDLECACHE *data)
{
	/* Delete the old cache triples.
	 * Note that our owl:sameAs statements take the form
	 * <external> owl:sameAs <proxy>, so we can delete <proxy> ?p ?o with
	 * impunity.
	 */	
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
	if(spindle_cache_store_s3_(data))
	{
		return -1;
	}
	return 0;
}

static int
spindle_cache_store_s3_(SPINDLECACHE *data)
{
	char *proxy, *source, *urlbuf, *t;
	size_t proxylen, sourcelen;
	char nqlenstr[256];
	S3REQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r;
	long status;

	if(!data->spindle->bucket)
	{
		return 0;
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
	memset(&s3data, 0, sizeof(struct s3_upload_struct));
	s3data.bufsize = proxylen + sourcelen + 1;
	s3data.buf = (char *) malloc(s3data.bufsize + 2);
	if(!s3data.buf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for consolidated N-Quads\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
		return -1;
	}
	memcpy(s3data.buf, proxy, proxylen);
	s3data.buf[proxylen] = '\n';
	memcpy(&(s3data.buf[proxylen + 1]), source, sourcelen);
	s3data.buf[s3data.bufsize] = 0;
	librdf_free_memory(proxy);
	librdf_free_memory(source);
	urlbuf = (char *) malloc(1 + strlen(data->localname) + 4 + 1);
	if(!urlbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for URL\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
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
	else
	{
		t = strchr(t, 0);
	}
	strcpy(t, ".nq");
	twine_logf(LOG_DEBUG, "bucket-relative URL is <%s>\n", urlbuf);
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
	if(s3_request_perform(req))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to upload N-Quads to bucket at <%s>\n", urlbuf);
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
							"  ?s <http://www.w3.org/2002/07/owl#sameAs> ?local .\n"
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

/* Initialise a data structure used to hold state while an individual proxy
 * entity is updated to reflect modified source data.
 */
static int
spindle_cache_init_(SPINDLECACHE *data, SPINDLE *spindle, const char *localname)
{	
	const char *t;

	memset(data, 0, sizeof(SPINDLECACHE));
	data->spindle = spindle;
	data->sparql = spindle->sparql;
	data->localname = localname;
	data->self = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) localname);
	if(!data->self)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for <%s>\n", localname);
		return -1;
	}
	if(spindle->multigraph)
	{
		t = strchr(localname, '#');
		if(!t)
		{
			t = strchr(localname, 0);
		}
		data->graph = librdf_new_node_from_counted_uri_string(spindle->world, (unsigned const char *) localname, t - localname);
	}
	else
	{
		data->graph = spindle->rootgraph;
	}
	data->sameas = spindle->sameas;
	if(!(data->sourcedata = twine_rdf_model_create()))
	{
		return -1;
	}
	if(!(data->proxydata = twine_rdf_model_create()))
	{
		return -1;
	}
	return 0;
}

/* Clean up the proxy entity update state data structure */
static int
spindle_cache_cleanup_(SPINDLECACHE *data)
{
	if(data->proxydata)
	{
		librdf_free_model(data->proxydata);
	}
	if(data->sourcedata)
	{
		librdf_free_model(data->sourcedata);
	}
	if(data->graph && data->graph != data->spindle->rootgraph)
	{
		librdf_free_node(data->graph);
	}
	if(data->self)
	{
		librdf_free_node(data->self);
	}
	return 0;
}
