#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libtwine.h"

size_t twine_cache_store_s3_upload_(char *buffer, size_t size, size_t nitems, void *userdata);
size_t twine_cache_fetch_s3_download_(char *buffer, size_t size, size_t nitems, void *userdata);
int add_node_to_list_(librdf_node *node, char ***array, size_t *array_size);

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
	char **subjects;
	char **objects;
	size_t nb_subjects;
	size_t nb_objects;
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *subject, *object;

	// We extract a list of all the URI which are a subject or an object in
	// this graph. This will go in the table "subject_objects" and be used
	// to search for related data
	subjects = NULL;
	objects = NULL;
	nb_subjects = 0;
	nb_objects = 0;
	st = librdf_model_as_stream(graph->store);
	while(!librdf_stream_end(st))
	{
		// Get the current statement from the stream
		statement = librdf_stream_get_object(st);

		// Look at the subject
		subject = librdf_statement_get_subject(statement);
		if (add_node_to_list_(subject, &subjects, &nb_subjects))
		{
			twine_logf(LOG_CRIT, "error indexing the subject\n");
			librdf_free_stream(st);
			return -1;
		}

		// Look at the object
		object = librdf_statement_get_object(statement);
		if (add_node_to_list_(object, &objects, &nb_objects))
		{
			twine_logf(LOG_CRIT, "error indexing the object\n");
			librdf_free_stream(st);
			return -1;
		}

		// Move to the next statement
		librdf_stream_next(st);
	}
	librdf_free_stream(st);
	twine_logf(LOG_DEBUG, "found %d subjects\n", nb_subjects);
	twine_logf(LOG_DEBUG, "found %d objects\n", nb_objects);

	// We now remove the previous entry for this graph, add a new empty entry
	// and append all the subjects and objects found
	// TODO optimise that code to send less SQL queries
	if(sql_executef(context->db, "DELETE FROM subject_objects WHERE \"graph\" = %Q", graph->uri))
	{
		twine_logf(LOG_CRIT, "could not remove the entry from the graph\n");
		return -2;
	}
	if(sql_executef(context->db, "INSERT INTO \"subject_objects\" (\"graph\", \"subjects\", \"objects\") VALUES (%Q, ARRAY[]::text[], ARRAY[]::text[])", graph->uri))
	{
		twine_logf(LOG_CRIT, "could not remove add an entry for the graph\n");
		return -2;
	}
	for (size_t i=0; i < nb_subjects; i++)
	{
		if(sql_executef(context->db, "UPDATE \"subject_objects\" SET \"subjects\" = array_append(\"subjects\", %Q) WHERE \"graph\" = %Q", subjects[i], graph->uri))
		{
			twine_logf(LOG_CRIT, "could not save the subject\n");
			return -2;
		}
		free(subjects[i]);
	}
	for (size_t i=0; i < nb_objects; i++)
	{
		if(sql_executef(context->db, "UPDATE \"subject_objects\" SET \"objects\" = array_append(\"objects\", %Q) WHERE \"graph\" = %Q", objects[i], graph->uri))
		{
			twine_logf(LOG_CRIT, "could not save the object\n");
			return -2;
		}
		free(objects[i]);
	}

	// Free the arrays of subjects and objects
	free(subjects);
	free(objects);

	return 0;
}


int
add_node_to_list_(librdf_node *node, char ***array, size_t *array_size)
{
	librdf_uri *node_uri;
	const char *node_uri_str;
	char *node_uri_str_dup;
	int match;

	// Extract the string of the URI
	if(!librdf_node_is_resource(node))
	{
		return 0;
	}
	node_uri = librdf_node_get_uri(node);
	if (!node_uri)
	{
		return -1;
	}
	node_uri_str = (const char *) librdf_uri_as_string(node_uri);
	if (!node_uri_str)
	{
		return -1;
	}

	// See if that string is already in the set
	match = 0;
	for(size_t c = 0; c < *array_size; c++)
	{
		if(!strcmp((*array)[c], node_uri_str))
		{
			match = 1;
			break;
		}
	}

	// If it was not append it to the set
	if(!match)
	{
		*array = realloc(*array, sizeof(char *)*(*array_size+1));
		if (!*array)
		{
			twine_logf(LOG_CRIT, "could not allocate memory\n");
			return -1;
		}
		node_uri_str_dup = malloc(sizeof(char *) * (strlen(node_uri_str) + 1));
		strcpy(node_uri_str_dup, node_uri_str);
		(*array)[*array_size] = node_uri_str_dup;
		*array_size = *array_size + 1;
	}

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
	SQL_STATEMENT *rs;

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

			// TODO Could we optimise that to send less queries ?

			// If it's the first time we see this subject first insert an empty list of objects
			rs = sql_queryf(twine_->db, "SELECT \"graph\", \"subject\" FROM \"target_media\" WHERE \"graph\" = %Q AND \"subject\" = %Q", graph->uri, subject_uri_str);
			if(!rs)
			{
				twine_logf(LOG_CRIT, "could not query the DB\n");
				return -1;
			}
			if (sql_stmt_eof(rs))
			{
				// Insert a blank set
				if(sql_executef(context->db, "INSERT INTO \"target_media\" (\"graph\", \"subject\", \"objects\") VALUES (%Q, %Q, ARRAY[]::text[])", graph->uri, subject_uri_str))
				{
					twine_logf(LOG_CRIT, "could not set an empty entry for the graph\n");
					return -1;
				}
			}
			sql_stmt_destroy(rs);

			// Then add this entry to the list of objects
			if(sql_executef(context->db, "UPDATE \"target_media\" SET \"objects\" = array_append(\"objects\", %Q) WHERE \"graph\" = %Q and \"subject\" = %Q", object_uri_str, graph->uri, subject_uri_str))
			{
				twine_logf(LOG_CRIT, "could not add an entry for the graph\n");
				return -1;
			}
		}

		// Move to the next statement
		librdf_stream_next(st);
	}
	librdf_free_stream(st);

	return 0;
}

/**
 * Takes a parameter the model to which to put data in, the name of the target
 * named graph and the name of the target proxy
 *
 * Replaces the query
 *
 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
 *  GRAPH <graph> {
 *   <proxy> ?p1 ?s .
 *   FILTER(?p1 = <http://xmlns.com/foaf/0.1/page>      ||
 *          ?p1 = <http://search.yahoo.com/mrss/player> ||
 *          ?p1 = <http://search.yahoo.com/mrss/content>)
 *  }
 *  GRAPH ?g {
 *   ?s ?p ?o .
 *  }
 *  FILTER(?g != <graph> && ?g != <http://localhost/>)
 * }
 *
 * That used to be in spindle/twine/generate/related.c
 */
int
twine_cache_fetch_media_(librdf_model *model, librdf_node *graph, librdf_node *proxy)
{
	librdf_uri *graph_uri, *proxy_uri;
	const char *graph_uri_str, *proxy_uri_str;
	SQL_STATEMENT *rs;
	char *target_graph;
	librdf_model *temp;
	librdf_node *context;
	librdf_stream *st;
	librdf_statement *statement;

	// Check the input
	if (!librdf_node_is_resource(graph) || !librdf_node_is_resource(proxy))
	{
		twine_logf(LOG_CRIT, "unacceptable input for twine_cache_fetch_media_\n");
		return -1;
	}

	// Extract target resources strings
	graph_uri = librdf_node_get_uri(graph);
	if (!graph_uri)
	{
		return -1;
	}
	graph_uri_str = (const char *) librdf_uri_as_string(graph_uri);
	if (!graph_uri_str)
	{
		return -1;
	}
	proxy_uri = librdf_node_get_uri(proxy);
	if (!proxy_uri)
	{
		return -1;
	}
	proxy_uri_str = (const char *) librdf_uri_as_string(proxy_uri);
	if (!proxy_uri_str)
	{
		return -1;
	}

	// Query for all the related media
	rs = sql_queryf(twine_->db, "SELECT unnest(\"objects\") AS \"object\" FROM \"target_media\" WHERE \"graph\" = %Q AND \"subject\" = %Q", graph_uri_str, proxy_uri_str);
	if(!rs)
	{
		twine_logf(LOG_CRIT, "could not query the DB\n");
		return -1;
	}

	// Load the graphs
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		// Get the graph name
		target_graph = sql_stmt_str(rs, 0);

		// Skip the graph if it's the same as the query graph
		if (!strcmp(target_graph, graph_uri_str))
		{
			continue;
		}

		// Load the graph
		temp = twine_rdf_model_create();
		if (twine_cache_fetch_graph_(temp, target_graph))
		{
			twine_logf(LOG_CRIT, "failed to load graph from the cache\n");
			return -1;
		}

		// Set the context node
		context = librdf_new_node_from_uri_string(twine_->world, (const unsigned char *)graph);

		// Add the statements to the model
		st = librdf_model_as_stream(temp);
		while (!librdf_stream_end(st))
		{
			// Get the statement and add it
			statement = librdf_stream_get_object(st);
			if (librdf_model_context_add_statement(model, context, statement))
			{
				twine_logf(LOG_CRIT, "could not add a statement\n");
				librdf_free_stream(st);
				return -1;
			}

			// Next!
			librdf_stream_next(st);
		}

		// Free the model, the context and the stream
		twine_rdf_model_destroy(temp);
		librdf_free_node(context);
	}
	sql_stmt_destroy(rs);

	return 0;
}


/**
 * Loads the content of the graph with the URI 'graph' and put the triples
 * into the model 'model'
 *
 * Replaces the query
 * SELECT DISTINCT ?s ?p ?o WHERE {
 *   GRAPH <uri> {
 *     ?s ?p ?o .
 *   }
 * }
 *
 * That used to be in spindle/twine/common/graphcache.c
 */
int
twine_cache_fetch_graph_(librdf_model *model, const char *uri)
{
	size_t l;
	char *tbuf;
	librdf_parser *parser;
	librdf_uri *base;
	int r;

	// Get the triples as a buffer, tbuf will be set to NULL in case of failure
	// or if the data is not available
	twine_cache_fetch_s3_(uri, &tbuf, &l);
	if (!tbuf)
	{
		twine_logf(LOG_DEBUG, "could not load any triples from the cache !\n");
	}
	else
	{
		// Create a parser
		parser = librdf_new_parser(twine_->world, "ntriples", "application/n-triples", NULL);
		if(!parser)
		{
			twine_logf(LOG_ERR, "failed to create a new parser\n");
			return -1;
		}

		// Configure it
		base = librdf_new_uri(twine_->world, (const unsigned char *) "/");
		if(!base)
		{
			librdf_free_parser(parser);
			twine_logf(LOG_CRIT, "failed to parse URI\n");
			return -1;
		}

		// Parse the buffer into the model
		r = librdf_parser_parse_counted_string_into_model(parser, (const unsigned char *) tbuf, l, base, model);
		if(r)
		{
			librdf_free_parser(parser);
			twine_logf(LOG_DEBUG, "failed to parse buffer\n");
			return -1;
		}

		free(tbuf);
		librdf_free_parser(parser);
	}

	return 0;
}


/**
 * Fetches the content of resource that relate to a particular 'uri'. The
 * content is loaded as quads into 'model'
 *
 * Replaces the query
 * SELECT DISTINCT ?s ?p ?o ?g WHERE {
 *   GRAPH ?g {
 *     { <uri> ?p ?o .
 *       BIND(<uri> as ?s)}
 *     UNION
 *     { ?s ?p <uri> .
 *       BIND(<uri> as ?o)}
 *   }
 * }
 *
 * That used to be in spindle/twine/generate/source.c
 */
int
twine_cache_fetch_about_(librdf_model *model, const char *uri)
{
	SQL_STATEMENT *rs;
	librdf_model *temp;
	char *graph;
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *subject, *object, *context;
	librdf_uri *subject_uri, *object_uri;
	const char *subject_uri_str, *object_uri_str;
	int subject_match, object_match;

	twine_logf(LOG_DEBUG, "calling twine_cache_fetch_about_ for %s\n", uri);

	// Find all the graphs that have 'uri' as a subject or object
	rs = sql_queryf(twine_->db, "SELECT \"graph\" FROM \"subject_objects\" WHERE %Q = ANY(\"subjects\") OR %Q = ANY(\"objects\")", uri, uri);
	if(!rs)
	{
		twine_logf(LOG_CRIT, "could not query the DB for graphs about %s\n", uri);
		return -1;
	}
	// Iterate over the results
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		// Load the graph
		graph = sql_stmt_str(rs, 0);
		temp = twine_rdf_model_create();
		if (twine_cache_fetch_graph_(temp, graph))
		{
			twine_logf(LOG_CRIT, "failed to load graph from the cache\n");
			return -1;
		}

		// Set the context node
		context = librdf_new_node_from_uri_string(twine_->world, (const unsigned char *)graph);

		// Iterate over the statements to add those using 'graph' as a subject
		// to the model. TODO if we find that the extra triples are not causing
		// any trouble we could instead directly all all the graph into the
		// target context
		st = librdf_model_as_stream(temp);
		while (!librdf_stream_end(st))
		{
			statement = librdf_stream_get_object(st);

			// Check if the subject matches
			subject_match = 0;
			subject = librdf_statement_get_subject(statement);
			if (librdf_node_is_resource(subject))
			{
				subject_uri = librdf_node_get_uri(subject);
				subject_uri_str = (const char *) librdf_uri_as_string(subject_uri);
				subject_match = !strcmp(subject_uri_str, uri);
			}

			// Check if the object matches
			object_match = 0;
			object = librdf_statement_get_object(statement);
			if (librdf_node_is_resource(object))
			{
				object_uri = librdf_node_get_uri(object);
				object_uri_str = (const char *) librdf_uri_as_string(object_uri);
				object_match = !strcmp(object_uri_str, uri);
			}

			if (subject_match || object_match)
			{
				if (librdf_model_context_add_statement(model, context, statement))
				{
					twine_logf(LOG_CRIT, "could not add a statement\n");
					librdf_free_stream(st);
					return -1;
				}
			}

			// Next!
			librdf_stream_next(st);
		}

		// Free the model, the context and the stream
		librdf_free_stream(st);
		twine_rdf_model_destroy(temp);
		librdf_free_node(context);
	}
	sql_stmt_destroy(rs);

	return 0;
}
