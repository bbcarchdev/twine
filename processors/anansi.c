/* Twine: Fetch a resource from an Anansi S3 bucket and process it.
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

#define _BSD_SOURCE                     1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <librdf.h>

#include "libtwine.h"
#include "liburi.h"
#include "libs3client.h"
#include "jsondata.h"

#define PLUGIN_NAME                     "Anansi"

struct bucketinfo_struct
{
	char *name;
	S3BUCKET *bucket;
};

struct ingestinfo_struct
{
	char *buf;
	size_t pos;
	size_t size;
};

static int process_anansi(const char *mime, const unsigned char *buf, size_t buflen, void *data);
static S3BUCKET *get_bucket(const char *name);
static S3BUCKET *add_bucket(struct bucketinfo_struct *info, const char *name);
static int ingest_info(S3BUCKET *bucket, const char *resource, jd_var *dict);
static int ingest_payload(S3BUCKET *bucket, const char *resource, const char *payload);
static int process_payload(const char *buf, size_t buflen, const char *type, const char *graph);
static size_t ingest_write(char *ptr, size_t size, size_t nemb, void *userdata);

static struct bucketinfo_struct bucketinfo[8];
static size_t maxbuckets = 8;

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	twine_plugin_register("application/x-anansi-url", "Anansi URL", process_anansi, NULL);
	return 0;
}

static int process_anansi(const char *mime, const unsigned char *buf, size_t buflen, void *data)
{
	char *str, *t;
	URI *uri;
	URI_INFO *info;
	S3BUCKET *bucket;
	int r;
	jd_var dict = JD_INIT;
	jd_var *loc;

	(void) mime;
	(void) data;

	/* Impose a hard limit on URL lengths */
	r = -1;
	if(buflen > 1024)
	{
		buflen = 1024;
	}
	str = (char *) calloc(1, buflen + 1);
	if(!str)
	{
		return -1;
	}
	memcpy((void *) str, (void *) buf, buflen);
	str[buflen] = 0;
	t = strchr(str, '\n');
	if(t)
	{
		*t = 0;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": URI is <%s>\n", str);
	uri = uri_create_str(str, NULL);
	if(!uri)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to parse <%s>\n", str);
		free(str);
		return -1;
	}
	info = uri_info(uri);
	if(!info->scheme || strcasecmp(info->scheme, "s3") || !info->host || !info->path)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": <%s> is not a valid S3 URL\n", str);
		uri_info_destroy(info);
		uri_destroy(uri);
		free(str);
		return -1;
	}
	JD_SCOPE
	{		
		bucket = get_bucket(info->host);
		if(bucket)
		{
			r = ingest_info(bucket, info->path, &dict);
			if(r || dict.type != HASH)
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to fetch cache information for <%s>\n", str);
			}
			else
			{
				loc = jd_get_ks(&dict, "location", 0);
				if(loc)
				{
					r = ingest_payload(bucket, info->path, jd_bytes(loc, NULL));
					if(r)
					{
						twine_logf(LOG_ERR, PLUGIN_NAME ": failed to ingest payload for <%s>\n", str);
					}
				}
			}
			jd_release(&dict);
		}
		else
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain bucket for <%s>\n", str);
		}
	}
	uri_info_destroy(info);
	uri_destroy(uri);
	free(str);
	return r;
}

static S3BUCKET *
get_bucket(const char *name)
{
	size_t c;
	
	for(c = 0; c < maxbuckets; c++)
	{
		if(bucketinfo[c].name && !strcmp(bucketinfo[c].name, name))
		{
			return bucketinfo[c].bucket;
		}
	}
	for(c = 0; c < maxbuckets; c++)
	{
		if(!bucketinfo[c].name)
		{
			return add_bucket(&(bucketinfo[c]), name);
		}
	}
    /* Recycle the oldest entry */
	free(bucketinfo[0].name);
	s3_destroy(bucketinfo[0].bucket);
	memmove(&(bucketinfo[0]), &(bucketinfo[1]), sizeof(struct bucketinfo_struct) * maxbuckets - 1);
	return add_bucket(&(bucketinfo[maxbuckets - 1]), name);
}

static S3BUCKET *
add_bucket(struct bucketinfo_struct *info, const char *name)
{
	char *t;

	memset(info, 0, sizeof(struct bucketinfo_struct));
	info->name = strdup(name);
	if(!info->name)
	{
		return NULL;
	}
	info->bucket = s3_create(name);
	if(!info->bucket)
	{
		free(info->name);
		info->name = NULL;
		return NULL;
	}
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		s3_set_endpoint(info->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		s3_set_access(info->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		s3_set_secret(info->bucket, t);
		free(t);
	}
	return info->bucket;
}

static int
ingest_info(S3BUCKET *bucket, const char *resource, jd_var *dict)
{
	S3REQUEST *req;
	CURL *ch;
	struct ingestinfo_struct info;
	long status;
	int r;
	char *urlbuf;

	urlbuf = (char *) malloc(strlen(resource) + 6);
	if(!urlbuf)
	{
		return -1;
	}
	strcpy(urlbuf, resource);
	strcat(urlbuf, ".json");
	memset(&info, 0, sizeof(struct ingestinfo_struct));
	req = s3_request_create(bucket, urlbuf, "GET");
	free(urlbuf);
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, ingest_write);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) &info);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, twine_config_get_bool("s3:verbose", 0));
	if(s3_request_perform(req) || !info.buf)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to request resource '%s'\n", resource);
		free(info.buf);
		s3_request_destroy(req);
		return -1;
	}
	status = 0;
	curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
	if(status != 200)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to request resource '%s' with status %ld\n", resource, status);
		free(info.buf);
		s3_request_destroy(req);
		return -1;
	}
	if(jd_from_jsons(dict, info.buf))
	{
		r = 0;
	}
	else
	{
		r = -1;
	}
	free(info.buf);
	s3_request_destroy(req);
	return r;
}

static int
ingest_payload(S3BUCKET *bucket, const char *resource, const char *location)
{
	S3REQUEST *req;
	CURL *ch;
	struct ingestinfo_struct info;
	long status;
	int r;
	char *type, *urlbuf;

	memset(&info, 0, sizeof(struct ingestinfo_struct));
	urlbuf = (char *) malloc(strlen(resource) + 9);
	if(!urlbuf)
	{
		return -1;
	}
	strcpy(urlbuf, resource);
	strcat(urlbuf, ".payload");
	req = s3_request_create(bucket, urlbuf, "GET");
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, ingest_write);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) &info);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, twine_config_get_bool("s3:verbose", 0));
	if(s3_request_perform(req) || !info.buf)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to request resource '%s'\n", resource);
		free(info.buf);
		s3_request_destroy(req);
		return -1;
	}
	status = 0;
	curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
	if(status != 200)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to request resource '%s' with status %ld\n", resource, status);
		free(info.buf);
		s3_request_destroy(req);
		return -1;
	}
	type = NULL;
	curl_easy_getinfo(ch, CURLINFO_CONTENT_TYPE, &type);
	if(!type)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to request resource '%s': no Content-Type in response\n", resource, status);
		free(info.buf);
		s3_request_destroy(req);
		return -1;
	}
	r = process_payload(info.buf, info.pos, type, location);
	free(info.buf);
	s3_request_destroy(req);
	return r;
}

static int
process_payload(const char *buf, size_t buflen, const char *type, const char *graph)
{
	librdf_model *model;
	librdf_stream *stream;
	int r;

	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create new RDF model\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": parsing buffer into model as '%s'\n", type);
	if(twine_rdf_model_parse(model, type, buf, buflen))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to parse string into model\n");
		librdf_free_model(model);
		return -1;
	}
	stream = librdf_model_as_stream(model);
	if(twine_sparql_put_stream(graph, stream))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to update graph <%s>\n", graph);
		r = 1;
	}
	else
	{
		r = 0;
	}
	librdf_free_stream(stream);
	librdf_free_model(model);
	return r;	
}

static size_t
ingest_write(char *ptr, size_t size, size_t nemb, void *userdata)
{
	struct ingestinfo_struct *info;
	char *p;

	info = (struct ingestinfo_struct *) userdata;
	
	size *= nemb;
	if(size >= info->size)
	{
		p = (char *) realloc(info->buf, info->size + size + 1);
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to reallocate buffer to %lu bytes\n", (unsigned long) (info->size + size + 1));
			return 0;
		}
		info->buf = p;
	}
	memcpy(&(info->buf[info->pos]), ptr, size);
	info->pos += size;
	info->buf[info->pos] = 0;
	return size;
}
