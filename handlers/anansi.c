/* Twine: Fetch a resource from an Anansi S3 bucket and process it.
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

#define _BSD_SOURCE                     1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
static const unsigned char *bulk_anansi(const char *mime, const unsigned char *buf, size_t buflen, void *data);
static S3BUCKET *get_bucket(const char *name);
static S3BUCKET *add_bucket(struct bucketinfo_struct *info, const char *name);
static int ingest_info(S3BUCKET *bucket, const char *resource, jd_var *dict);
static int ingest_payload(S3BUCKET *bucket, const char *resource, const char *payload, librdf_model *model);
static int process_payload(const char *buf, size_t buflen, const char *type, const char *graph, librdf_model *model);
static size_t ingest_write(char *ptr, size_t size, size_t nemb, void *userdata);
static int ingest_headers(jd_var *dict, const char *graph, librdf_model *model);
static int ingest_link(librdf_world *world, librdf_model *model, const char *value, librdf_uri *resource);

static struct bucketinfo_struct bucketinfo[8];
static size_t maxbuckets = 8;

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	twine_plugin_register("application/x-anansi-url", "Anansi URL", process_anansi, NULL);
	twine_bulk_register("application/x-anansi-url", "Anansi URL", bulk_anansi, NULL);
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
	jd_var *loc, *headers;
	librdf_model *model;
	const char *location;

	(void) mime;
	(void) data;

	/* Impose a hard limit on URL lengths */
	r = 0;
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
		r = -1;
	}
	if(!r)
	{
		model = twine_rdf_model_create();
		if(!model)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create new RDF model\n");
			r = -1;
		}
	}
	if(!r)
	{
		bucket = get_bucket(info->host);
		if(!bucket)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain bucket for <%s>\n", str);
			r = -1;	   
		}
	}
	JD_SCOPE
	{
		if(!r)
		{
			r = ingest_info(bucket, info->path, &dict);
			if(r || dict.type != HASH)
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to fetch cache information for <%s>\n", str);
				r = -1;
			}
		}
		if(!r)
		{
			loc = jd_get_ks(&dict, "content_location", 0);
			if(!loc)
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": object has no Content-Location\n");
				r = -1;
			}
			location = jd_bytes(loc, NULL);
		}
		if(!r)
		{
			r = ingest_payload(bucket, info->path, location, model);
			if(r)
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to ingest payload for <%s>\n", str);
			}
		}
		if(!r)
		{
			headers = jd_get_ks(&dict, "headers", 0);
			if(headers && headers->type == HASH)
			{
				if((r = ingest_headers(headers, location, model)))
				{
					twine_logf(LOG_ERR, PLUGIN_NAME ": failed to process headers\n");
				}
			}
		}
		if(!r)
		{
			if(twine_sparql_put_model(location, model))
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to update graph <%s>\n", location);
				r = -1;
			}
		}
		jd_release(&dict);
	}
	librdf_free_model(model);
	uri_info_destroy(info);
	uri_destroy(uri);
	free(str);
	return r;
}

static unsigned char *
ustrnchr(const unsigned char *src, int ch, size_t max)
{
	const unsigned char *t;

	for(t = src; (size_t) (t - src) < max; t++)
	{
		if(!*t)
		{
			break;
		}
		if(*t == ch)
		{
			return (unsigned char *) t;
		}
	}
	return NULL;
}

static const unsigned char *
bulk_anansi(const char *mime, const unsigned char *buf, size_t buflen, void *data)
{
	const unsigned char *start, *t;
	size_t remaining;

	t = buf;
	while((size_t) (t - buf) < buflen)
	{
		start = t;
		remaining = buflen - (t - buf);
		t = ustrnchr(start, '\n', remaining);
		if(t == start)
		{
			continue;
		}
		if(!t)
		{
			return (const unsigned char *) start;
		}
		if(process_anansi(mime, start, t - start, data))
		{
			return NULL;
		}
		t++;
	}
	return t;
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
ingest_payload(S3BUCKET *bucket, const char *resource, const char *location, librdf_model *model)
{
	S3REQUEST *req;
	CURL *ch;
	struct ingestinfo_struct info;
	long status;
	int r;
	char *type;

	memset(&info, 0, sizeof(struct ingestinfo_struct));
	req = s3_request_create(bucket, resource, "GET");
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
	r = process_payload(info.buf, info.pos, type, location, model);
	free(info.buf);
	s3_request_destroy(req);
	return r;
}

static int
process_payload(const char *buf, size_t buflen, const char *type, const char *graph, librdf_model *model)
{
	librdf_world *world;
	librdf_uri *base;

	world = twine_rdf_world();
	base = librdf_new_uri(world, (const unsigned char *) graph);
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": parsing buffer into model as '%s'\n", type);
	if(twine_rdf_model_parse_base(model, type, buf, buflen, base))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to parse string into model\n");
		librdf_free_uri(base);
		return -1;
	}
	librdf_free_uri(base);
	return 0;	
}

static size_t
ingest_write(char *ptr, size_t size, size_t nemb, void *userdata)
{
	struct ingestinfo_struct *info;
	char *p;

	info = (struct ingestinfo_struct *) userdata;
	
	size *= nemb;
	if(size >= (info->size - info->pos))
	{
		p = (char *) realloc(info->buf, info->size + size + 1);
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to reallocate buffer to %lu bytes\n", (unsigned long) (info->size + size + 1));
			return 0;
		}
		info->buf = p;
		info->size += size;
	}
	memcpy(&(info->buf[info->pos]), ptr, size);
	info->pos += size;
	info->buf[info->pos] = 0;
	return size;
}

static int
ingest_headers(jd_var *dict, const char *graph, librdf_model *model)
{
	jd_var keys = JD_INIT, *ks, *array, *str;
	size_t c, d, num, anum;
	const char *value;
	librdf_world *world;
	librdf_uri *resource;
	
	world = twine_rdf_world();
	resource = librdf_new_uri(world, (const unsigned char *) graph);
	jd_keys(&keys, dict);
	num = jd_count(&keys);
	for(c = 0; c < num; c++)
	{
		ks = jd_get_idx(&keys, c);
		if(!ks || ks->type != STRING)
		{
			continue;
		}
		if(!strcasecmp(jd_bytes(ks, NULL), "link"))
		{
			array = jd_get_key(dict, ks, 0);
			if(array && array->type == ARRAY)
			{
				anum = jd_count(array);
				for(d = 0; d < anum; d++)
				{
					str = jd_get_idx(array, d);
					if(!str || str->type != STRING)
					{
						continue;
					}
					value = jd_bytes(str, NULL);
					ingest_link(world, model, value, resource);
				}
			}
		}
	}	
	return 0;
}

static int
ingest_link(librdf_world *world, librdf_model *model, const char *value, librdf_uri *resource)
{
	static const char *relbase = "http://www.w3.org/1999/xhtml/vocab#";
	const char *t, *pend, *vstart, *s;
	char *anchorstr, *uristr, *relstr, *p;
	librdf_uri *anchor, *uri, *rel;
	int q, abs;
	librdf_node *subject, *predicate, *object;
	librdf_statement *st;

	rel = NULL;
	while(*value)
	{
		anchorstr = NULL;
		uristr = NULL;
		relstr = NULL;
		t = value;
		while(*t && isspace(*t))
		{
			t++;
		}
		if(*t != '<')
		{
			twine_logf(LOG_NOTICE, PLUGIN_NAME ": ignoring malformed Link header (%s)\n", value);
			return -1;
		}
		value = t + 1;
		while(*t && *t != '>')
		{
			t++;
		}
		if(!*t)
		{
			twine_logf(LOG_NOTICE, PLUGIN_NAME ": ignoring malformed Link header (%s)\n", value);
			return -1;
		}
		uristr = (char *) malloc(t - value + 1);
		if(!uristr)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to allocate memory for Link URI\n");
			return -1;
		}
		strncpy(uristr, value, t - value);
		uristr[t - value] = 0;
		value = t + 1;		
		while(*value && *value != ',')
		{
			vstart = NULL;
			q = 0;
			while(*value == ' ' || *value == '\t')
			{
				value++;
			}
			if(!*value)
			{
				break;
			}
			t = value;
			/* Parse a single parameter */
			while(*t)
			{
				if(*t == '=' || *t == ';')
				{
					break;
				}
				if(*t == ' ' || *t == '\t')
				{
					twine_logf(LOG_NOTICE, PLUGIN_NAME ": ignoring link relation with malformed parameters ('%s')\n", value);
					return -1;
				}
				t++;
			}
			if(!*t || *t == ',')
			{
				break;
			}
			if(*t == ';')
			{
				t++;
				value = t;
				continue;
			}
			pend = t;
			t++;
			while(*t == ' ' || *t == '\t')
			{
				t++;
			}
			vstart = t;
			while(*t)
			{
				if(q)
				{
					if(*t == q)
					{
						q = 0;
					}
					t++;
					continue;				
				}
				if(*t == '"')
				{
					q = *t;
					t++;
					continue;
				}
				if(*t == ';')
				{
					break;
				}
				if(*t == ',')
				{
					break;
				}
				t++;
			}		
			/* Parse a 'rel' parameter */
			if(!relstr && pend - value == 3 && !strncmp(value, "rel", 3))
			{
				/* If the relation is not something that looks like a URI,
				 * create one by concatenating it to relbase; otherwise,
				 * just parse the relation as a URI.
				 */
				relstr = (char *) malloc(t - vstart + strlen(relbase) + 1);
				p = relstr;
				abs = 0;
				for(s = vstart; s < t; s++)
				{
					if(*s == ':' || *s == '/')
					{
						abs = 1;
						break;
					}
				}
				if(!abs)
				{
					strcpy(relstr, relbase);
					p = strchr(relstr, 0);
				}
				for(s = vstart; s < t; s++)
				{
					if(*s == '"')
					{
						continue;
					}
					*p = *s;
					p++;
				}
				*p = 0;
			}
			else if(!anchorstr && pend - value == 6 && !strncmp(value, "anchor", 6))
			{
				anchorstr = (char *) malloc(t - vstart + 1);
				p = anchorstr;
				for(s = vstart; s < t; s++)
				{
					if(*s == '"')
					{
						continue;
					}
					*p = *s;
					p++;
				}
				*p = 0;
			}
			value = t;
			if(!*value || *value == ',')
			{
				break;
			}
			value++;
		}
		/* We have now parsed all parameters */
		anchor = NULL;
		rel = NULL;
		uri = NULL;
		if(anchorstr)
		{
			anchor = librdf_new_uri_relative_to_base(resource, (const unsigned char *) anchorstr);
		}
		else
		{
			anchor = resource;
		}
		if(relstr)
		{
			uri = librdf_new_uri_relative_to_base(anchor, (const unsigned char *) uristr);
			
			/* Only process links which actually have a relation */
			rel = librdf_new_uri(world, (const unsigned char *) relstr);
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": Link <%s> <%s> <%s>\n",
					   (const char *) librdf_uri_to_string(anchor),
					   (const char *) librdf_uri_to_string(rel),
					   (const char *) librdf_uri_to_string(uri));
			/* Create a new triple (content-location, relation, target) */
			subject = librdf_new_node_from_uri(world, anchor);
			predicate = librdf_new_node_from_uri(world, rel);
			object = librdf_new_node_from_uri(world, uri);
			st = librdf_new_statement_from_nodes(world, subject, predicate, object);
			/* Add the triple to the model */
			librdf_model_add_statement(model, st);
			librdf_free_statement(st);
			librdf_free_uri(rel);
			librdf_free_uri(uri);
		}
		if(anchor && anchor != resource)
		{
			librdf_free_uri(anchor);
		}
		free(anchorstr);
		anchorstr = NULL;
		free(relstr);
		relstr = NULL;
		free(uristr);
		uristr = NULL;
		
		if(*value)
		{
			value++;
		}
	}
	return 0;
}
