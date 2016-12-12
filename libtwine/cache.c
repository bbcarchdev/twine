#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libtwine.h"

/* Utility function for interfacing with S3 and the DB */
size_t twine_cache_store_s3_upload_(char *buffer, size_t size, size_t nitems, void *userdata);
size_t twine_cache_fetch_s3_download_(char *buffer, size_t size, size_t nitems, void *userdata);

struct s3_upload_struct
{
	char *buf;
	size_t bufsize;
	size_t pos;
};


int
twine_cache_store_s3_(const char *g, char *ntbuf, size_t bufsize)
{
	char nqlenstr[256];
	AWSREQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;
	char *t;
	AWSS3BUCKET *bucket;

	// Set the data to send
	s3data.buf = ntbuf;
	s3data.bufsize = bufsize;
	s3data.pos = 0;

	// Create a bucket "twine"
	bucket = aws_s3_create("twine");
	aws_s3_set_logger(bucket, twine_vlogf);
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		aws_s3_set_endpoint(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		aws_s3_set_access(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		aws_s3_set_secret(bucket, t);
		free(t);
	}

	// Prepare the request
	req = aws_s3_request_create(bucket, g, "PUT");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(ch, CURLOPT_READFUNCTION, twine_cache_store_s3_upload_);
	curl_easy_setopt(ch, CURLOPT_READDATA, &s3data);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE, (long) s3data.bufsize);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
	headers = curl_slist_append(aws_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Content-Type: " MIME_NQUADS);
	headers = curl_slist_append(headers, "x-amz-acl: public-read");
	sprintf(nqlenstr, "Content-Length: %u", (unsigned) s3data.bufsize);
	headers = curl_slist_append(headers, nqlenstr);
	aws_request_set_headers(req, headers);
	r = 0;
	twine_logf(LOG_DEBUG,"Request ok\n");
	if((e = aws_request_perform(req)))
	{
		twine_logf(LOG_ERR, "failed to upload buffer to bucket : %s\n", curl_easy_strerror(e));
		r = -1;
	}
	else
	{
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status != 200)
		{
			twine_logf(LOG_ERR, "failed to upload buffer to bucket : HTTP status %ld\n", status);
			r = -1;
		}
	}
	aws_request_destroy(req);
	aws_s3_destroy(bucket);

	return r;
}

int
twine_cache_fetch_s3_(const char *g, char **ntbuf, size_t *buflen)
{
	AWSREQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;
	char *t;
	AWSS3BUCKET *bucket;

	// Init the structures that will be returned
	*ntbuf = NULL;
	*buflen = 0;
	memset(&s3data, 0, sizeof(struct s3_upload_struct));

	// Create a bucket "twine"
	bucket = aws_s3_create("twine");
	aws_s3_set_logger(bucket, twine_vlogf);
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		aws_s3_set_endpoint(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		aws_s3_set_access(bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		aws_s3_set_secret(bucket, t);
		free(t);
	}

	// Prepare the request
	req = aws_s3_request_create(bucket, g, "GET");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, twine_cache_fetch_s3_download_);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, &s3data);
	headers = curl_slist_append(aws_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Accept: " MIME_NQUADS);
	aws_request_set_headers(req, headers);
	r = 1;
	if((e = aws_request_perform(req)))
	{
		twine_logf(LOG_ERR, "failed to download buffer from bucket : %s\n", curl_easy_strerror(e));
		r = -1;
	}
	else
	{
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status == 404 || status == 403)
		{
			twine_logf(LOG_DEBUG, "not found\n");
			r = 0;
		}
		else if(status != 200)
		{
			twine_logf(LOG_ERR, "failed to download buffer from bucket : HTTP status %ld\n", status);
			r = -1;
		}
	}
	aws_request_destroy(req);
	if(r > 0)
	{
		twine_logf(LOG_DEBUG, "all fine!\n");
		*ntbuf = s3data.buf;
		*buflen = s3data.pos;
	}
	else
	{
		free(s3data.buf);
	}
	aws_s3_destroy(bucket);

	return r;
}

size_t
twine_cache_store_s3_upload_(char *buffer, size_t size, size_t nitems, void *userdata)
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

size_t
twine_cache_fetch_s3_download_(char *buffer, size_t size, size_t nitems, void *userdata)
{
	struct s3_upload_struct *data;
	char *p;

	data = (struct s3_upload_struct *) userdata;
	size *= nitems;
	if(data->pos + size >= data->bufsize)
	{
		p = (char *) realloc(data->buf, data->bufsize + size + 1);
		if(!p)
		{
			twine_logf(LOG_CRIT, "failed to expand receive buffer\n");
			return 0;
		}
		data->buf = p;
		data->bufsize += size;
	}
	memcpy(&(data->buf[data->pos]), buffer, size);
	data->pos += size;
	data->buf[data->pos] = 0;
	twine_logf(LOG_DEBUG, "read %lu bytes\n", (unsigned long) size);
	return size;
}

/** Index 1 : Find all the triples having <X> as a subject or object
 * return all the triples and the name of the source they came from
 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
 * GRAPH ?g {
 *  { <X> ?p ?o .
 *  BIND(<X> as ?s)  }
 *  UNION
 *  { ?s ?p <X> .
 *  BIND(<X> as ?o)  }
 * }
 * }
 *
 * Table => Graph | {subject/object}
 *
 * Query => Get the list of graphs, fetch them from S3, filter relevant triples
**/
int
twine_cache_index_subject_objects_(TWINE *restrict context, TWINEGRAPH *restrict graph)
{
	char **subject_objects;
	size_t nb_subject_objects;
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *node;
	librdf_node *nodes[2];
	librdf_uri *node_uri;
	const char *node_uri_str;
	char *node_uri_str_dup;
	int match;

	// We extract a list of all the URI which are a subject or an object in
	// this graph. This will go in the table "subject_objects" and be used
	// to search for related data
	subject_objects = NULL;
	nb_subject_objects = 0;
	st = librdf_model_as_stream(graph->store);
	while(!librdf_stream_end(st))
	{
		// Get the current statement from the stream
		statement = librdf_stream_get_object(st);

		// Look at the subject and objects
		nodes[0] = librdf_statement_get_subject(statement);
		nodes[1] = librdf_statement_get_object(statement);

		// See if the subjects and objects could be added to the list
		for (size_t i=0; i < 2; i++)
		{
			// Extract the string of the URI
			node = nodes[i];
			if(!librdf_node_is_resource(node))
			{
				librdf_stream_next(st);
				continue;
			}
			node_uri = librdf_node_get_uri(node);
			if (!node_uri)
			{
				librdf_stream_next(st);
				continue;
			}
			node_uri_str = (const char *) librdf_uri_as_string(node_uri);
			if (!node_uri_str)
			{
				librdf_stream_next(st);
				continue;
			}

			// See if that string is already in the set
			match = 0;
			for(size_t c = 0; c < nb_subject_objects; c++)
			{
				if(!strcmp(subject_objects[c], node_uri_str))
				{
					match = 1;
					break;
				}
			}

			// If it was not append it to the set
			if(!match)
			{
				subject_objects = realloc(subject_objects, sizeof(char *)*(nb_subject_objects+1));
				if (!subject_objects)
				{
					twine_logf(LOG_CRIT, "could not allocate memory\n");
					librdf_free_stream(st);
					return -1;
				}
				node_uri_str_dup = malloc(sizeof(char *) * (strlen(node_uri_str) + 1));
				strcpy(node_uri_str_dup, node_uri_str);
				subject_objects[nb_subject_objects] = node_uri_str_dup;
				nb_subject_objects = nb_subject_objects + 1;
			}
		}

		// Move to the next statement
		librdf_stream_next(st);
	}
	librdf_free_stream(st);
	twine_logf(LOG_DEBUG, "found %d subject/objects\n", nb_subject_objects);

	// We now remove the previous entry for this graph, add a new empty entry
	// and append all the subjects and objects found
	// TODO optimise that code to send less SQL queries
	if(sql_executef(context->db, "DELETE FROM subject_objects WHERE \"graph\" = %Q", graph->uri))
	{
		twine_logf(LOG_CRIT, "could not remove the entry from the graph\n");
		return -2;
	}
	if(sql_executef(context->db, "INSERT INTO \"subject_objects\" (\"graph\", \"uris\") VALUES (%Q, ARRAY[]::text[])", graph->uri))
	{
		twine_logf(LOG_CRIT, "could not remove add an entry for the graph\n");
		return -2;
	}
	for (size_t i=0; i < nb_subject_objects; i++)
	{
		if(sql_executef(context->db, "UPDATE \"subject_objects\" SET \"uris\" = array_append(\"uris\", %Q) WHERE \"graph\" = %Q", subject_objects[i], graph->uri))
		{
			twine_logf(LOG_CRIT, "could not save the subject/object\n");
			return -2;
		}
	}

	// We won't need the string anymore so best to free them
	for (size_t i=0; i < nb_subject_objects; i++)
	{
		free(subject_objects[i]);
	}
	free(subject_objects);
	nb_subject_objects = 0;

	return 0;
}

/** Index 2 : For a given graph find all the media pointed at. Then
 * fetch their descriptions too. The index actually concerns the first
 * part of the query only
 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
 *  GRAPH <http://localhost/a9d3d9dac7804f4689789e455365b6c4> {
 *   <http://localhost/a9d3d9dac7804f4689789e455365b6c4#id> ?p1 ?s .
 *   FILTER(?p1 = <http://xmlns.com/foaf/0.1/page> ||
 *   ?p1 = <http://search.yahoo.com/mrss/player> ||
 *   ?p1 = <http://search.yahoo.com/mrss/content>)
 *  }
 *  GRAPH ?g {
 *   ?s ?p ?o .
 *  }
 *  FILTER(?g != <http://localhost/a9d3d9dac7804f4689789e455365b6c4> &&
 *  ?g != <http://localhost/>)
 * }
 * Table => Graph | Subject | link_type | target_media
 * Query => Get the list of link_type+target_media, create triples,
 * fetch target descriptions and add them to the model
**/
int
twine_cache_index_media_(TWINE *restrict context, TWINEGRAPH *restrict graph)
{
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *subject, *predicate, *object;
	librdf_uri *subject_uri, *predicate_uri, *object_uri;
	const char *subject_uri_str, *predicate_uri_str, *object_uri_str;
	const char *media_predicates[] = {"http://xmlns.com/foaf/0.1/page",
			"http://search.yahoo.com/mrss/player",
			"http://search.yahoo.com/mrss/content"};

	// Remove all the previous entries for this resource
	if(sql_executef(context->db, "DELETE FROM target_media WHERE \"graph\" = %Q", graph->uri))
	{
		twine_logf(LOG_CRIT, "could not remove the media entries for the graph\n");
		return -1;
	}

	// Iterate over all the triples to find the resources having a
	// foaf:page, mrss:player or mrss:content link to a resource
	st = librdf_model_as_stream(graph->store);
	while(!librdf_stream_end(st))
	{
		// Get the current statement from the stream
		statement = librdf_stream_get_object(st);

		// Get the subject, predicate and object
		subject = librdf_statement_get_subject(statement);
		predicate = librdf_statement_get_predicate(statement);
		object = librdf_statement_get_object(statement);

		// Continue if any of them is not a resource
		if(!librdf_node_is_resource(subject) ||
		   !librdf_node_is_resource(predicate) ||
		   !librdf_node_is_resource(object))
		{
			librdf_stream_next(st);
			continue;
		}

		// Extract the string of the predicate
		predicate_uri = librdf_node_get_uri(predicate);
		if (!predicate_uri)
		{
			librdf_stream_next(st);
			continue;
		}
		predicate_uri_str = (const char *) librdf_uri_as_string(predicate_uri);
		if (!predicate_uri_str)
		{
			librdf_stream_next(st);
			continue;
		}

		// See if it's one we like
		if(!strcmp(predicate_uri_str, media_predicates[0]) ||
		   !strcmp(predicate_uri_str, media_predicates[1]) ||
		   !strcmp(predicate_uri_str, media_predicates[2]))
		{
			twine_logf(LOG_DEBUG, "found a media linked with <%s>\n", predicate_uri_str);

			// Extract the string of the subject
			subject_uri = librdf_node_get_uri(subject);
			if (!subject_uri)
			{
				librdf_stream_next(st);
				continue;
			}
			subject_uri_str = (const char *) librdf_uri_as_string(subject_uri);
			if (!subject_uri_str)
			{
				librdf_stream_next(st);
				continue;
			}

			// Do the same for the object
			object_uri = librdf_node_get_uri(object);
			if (!object_uri)
			{
				librdf_stream_next(st);
				continue;
			}
			object_uri_str = (const char *) librdf_uri_as_string(object_uri);
			if (!object_uri_str)
			{
				librdf_stream_next(st);
				continue;
			}

			// Insert the entry
			if(sql_executef(context->db, "INSERT INTO \"target_media\" (\"graph\", \"subject\", \"predicate\", \"object\") VALUES (%Q, %Q, %Q, %Q)", graph->uri, subject_uri_str, predicate_uri_str, object_uri_str))
			{
				twine_logf(LOG_CRIT, "could not remove add an entry for the graph\n");
				return -1;
			}
		}

		// Move to the next statement
		librdf_stream_next(st);
	}
	librdf_free_stream(st);

	return 0;
}
