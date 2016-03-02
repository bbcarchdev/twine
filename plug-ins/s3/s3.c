/* Twine: Fetch a resource from S3 and then pass it back to Twine for
 * processing.
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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

#include "libtwine.h"
#include "liburi.h"
#include "libawsclient.h"

#define TWINE_PLUGIN_NAME               "s3"

struct bucketinfo_struct
{
	char *name;
	AWSS3BUCKET *bucket;
};

struct ingestinfo_struct
{
	char *buf;
	size_t pos;
	size_t size;
};

static int process_s3(const char *mime, const unsigned char *buf, size_t buflen, void *data);
static AWSS3BUCKET *get_bucket(const char *name);
static AWSS3BUCKET *add_bucket(struct bucketinfo_struct *info, const char *name);
static int ingest_resource(AWSS3BUCKET *bucket, const char *resource);
static size_t ingest_write(char *ptr, size_t size, size_t nemb, void *userdata);

static struct bucketinfo_struct bucketinfo[8];
static size_t maxbuckets = 8;

/* Twine plug-in entry-point */
int
twine_entry(TWINE *context, TWINEENTRYTYPE type, void *handle)
{
	(void) context;
	(void) handle;
	
	switch(type)
	{
	case TWINE_ATTACHED:
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME " plug-in: initialising\n");
		twine_plugin_register("application/x-s3-url", "S3 URL", process_s3, NULL);
		break;
	case TWINE_DETACHED:
		break;
	}
	return 0;
}

static int process_s3(const char *mime, const unsigned char *buf, size_t buflen, void *data)
{
	char *str, *t;
	URI *uri;
	URI_INFO *info;
	AWSS3BUCKET *bucket;
	int r;

	(void) mime;
	(void) data;

	/* Impose a hard limit on URL lengths */
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
	twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": URI is <%s>\n", str);
	uri = uri_create_str(str, NULL);
	if(!uri)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to parse <%s>\n", str);
		free(str);
		return -1;
	}
	info = uri_info(uri);
	if(!info->scheme || strcasecmp(info->scheme, "s3") || !info->host || !info->path)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": <%s> is not a valid S3 URL\n", str);
		uri_info_destroy(info);
		uri_destroy(uri);
		free(str);
		return -1;
	}
	bucket = get_bucket(info->host);
	if(!bucket)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to obtain bucket for <%s>\n", str);
		uri_info_destroy(info);
		uri_destroy(uri);
		free(str);
		return -1;
	}
	r = ingest_resource(bucket, info->path);
	uri_info_destroy(info);
	uri_destroy(uri);
	free(str);
	return r;
}

static AWSS3BUCKET *
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
	aws_s3_destroy(bucketinfo[0].bucket);
	memmove(&(bucketinfo[0]), &(bucketinfo[1]), sizeof(struct bucketinfo_struct) * maxbuckets - 1);
	return add_bucket(&(bucketinfo[maxbuckets - 1]), name);
}

static AWSS3BUCKET *
add_bucket(struct bucketinfo_struct *info, const char *name)
{
	char *t;

	memset(info, 0, sizeof(struct bucketinfo_struct));
	info->name = strdup(name);
	if(!info->name)
	{
		return NULL;
	}
	info->bucket = aws_s3_create(name);
	if(!info->bucket)
	{
		free(info->name);
		info->name = NULL;
		return NULL;
	}
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		aws_s3_set_endpoint(info->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		aws_s3_set_access(info->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		aws_s3_set_secret(info->bucket, t);
		free(t);
	}
	return info->bucket;
}

static int
ingest_resource(AWSS3BUCKET *bucket, const char *resource)
{
	AWSREQUEST *req;
	CURL *ch;
	struct ingestinfo_struct info;
	long status;
	int r;
	char *type;

	memset(&info, 0, sizeof(struct ingestinfo_struct));
	req = aws_s3_request_create(bucket, resource, "GET");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, ingest_write);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) &info);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, twine_config_get_bool("s3:verbose", 0));
	if(aws_request_perform(req) || !info.buf)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to request resource '%s'\n", resource);
		free(info.buf);
		aws_request_destroy(req);
		return -1;
	}
	status = 0;
	curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
	if(status != 200)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to request resource '%s' with status %ld\n", resource, status);
		free(info.buf);
		aws_request_destroy(req);
		return -1;
	}
	type = NULL;
	curl_easy_getinfo(ch, CURLINFO_CONTENT_TYPE, &type);
	if(!type)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to request resource '%s': no Content-Type in response\n", resource, status);
		free(info.buf);
		aws_request_destroy(req);
		return -1;
	}
	r = twine_plugin_process(type, (const unsigned char *) info.buf, info.pos, NULL);
	free(info.buf);
	aws_request_destroy(req);
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
			twine_logf(LOG_CRIT, TWINE_PLUGIN_NAME ": failed to reallocate buffer to %lu bytes\n", (unsigned long) (info->size + size + 1));
			return 0;
		}
		info->buf = p;
	}
	memcpy(&(info->buf[info->pos]), ptr, size);
	info->pos += size;
	info->buf[info->pos] = 0;
	return size;
}
