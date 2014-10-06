/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
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

#include "p_libs3client.h"

/* Create a new request for a resource within a bucket */
S3REQUEST *
s3_request_create(S3BUCKET *bucket, const char *resource, const char *method)
{
	S3REQUEST *p;

	p = (S3REQUEST *) calloc(1, sizeof(S3REQUEST));
	if(!p)
	{
		return NULL;
	}
	p->bucket = bucket;
	p->resource = strdup(resource);
	p->method = strdup(method);
	if(!p->resource || !p->method)
	{
		s3_request_destroy(p);
		return NULL;
	}
	return p;
}

/* Destroy a request */
int
s3_request_destroy(S3REQUEST *req)
{
	if(!req)
	{
		errno = EINVAL;
		return -1;
	}
	free(req->resource);
	free(req->method);
	if(req->ch)
	{
		curl_easy_cleanup(req->ch);
	}
	if(req->headers)
	{
		curl_slist_free_all(req->headers);
	}
	free(req);
	return 0;
}

/* Finalise (sign) a request */
int
s3_request_finalise(S3REQUEST *req)
{
	CURL *ch;
	struct curl_slist *headers;
	size_t l;
	char *resource, *url, *p;
	const char *t;

	if(req->finalised || !req->bucket->access || !req->bucket->secret || !req->bucket->bucket)
	{
		errno = EINVAL;
		return -1;
	}
	ch = s3_request_curl(req);
	if(!ch)
	{
		return -1;
	}
	/* The resource path is signed in the request, and takes the form:
	 * /{bucket}/[{basepath}]/{resource}
	 */
	l = 1 + strlen(req->bucket->bucket) + 1 + (req->bucket->basepath ? strlen(req->bucket->basepath) : 0) + 1 + strlen(req->resource) + 1;
	resource = (char *) calloc(1, l);
	if(!resource)
	{
		return -1;
	}
	p = resource;
	*p = '/';
	p++;
	strcpy(p, req->bucket->bucket);
	p += strlen(req->bucket->bucket);
	*p = '/';
	p++;
	t = NULL;
	if(req->bucket->basepath)
	{
		t = req->bucket->basepath;
		while(*t == '/')
		{
			t++;
		}
		if(!*t)
		{
			t = NULL;
		}
	}
	if(t)
	{
		strcpy(p, t);
		p += strlen(t) - 1;
		if(*p != '/')
		{
			p++;
			*p = '/';
		}
		p++;
	}
	t = req->resource;
	while(*t == '/')
	{
		t++;
	}
	strcpy(p, t);
	/* The URL is s3://{endpoint}{resource} */	
	l += 7 + strlen(req->bucket->endpoint) + 1;
	url = (char *) calloc(1, l);
	if(!url)
	{
		free(resource);
		return -1;
	}
	p = url;
	strcpy(p, "http://");
	p += 7;
	strcpy(p, req->bucket->endpoint);
	p += strlen(req->bucket->endpoint);
	strcpy(p, resource);
	
	headers = s3_sign(req->method, resource, req->bucket->access, req->bucket->secret, s3_request_headers(req));
	if(!headers)
	{
		return -1;
	}
	req->finalised = 1;
	req->headers = headers;
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, req->method);
	free(resource);
	free(url);
	return 0;
}

/* Perform a request, finalising if needed */
int
s3_request_perform(S3REQUEST *req)
{
	if(!req->finalised)
	{
		if(s3_request_finalise(req))
		{
			return -1;
		}
	}
	if(curl_easy_perform(req->ch) != CURLE_OK)
	{
		return -1;
	}
	return 0;
}

/* Obtain (creating if needed) the cURL handle for this request */
CURL *
s3_request_curl(S3REQUEST *request)
{
	if(!request->ch)
	{
		request->ch = curl_easy_init();
		if(!request->ch)
		{
			return NULL;
		}
	}
	return request->ch;
}

/* Obtain the headers list for this request */
struct curl_slist *
s3_request_headers(S3REQUEST *request)
{
	return request->headers;
}

/* Set the headers list for this request (the list will be freed upon
 * request destruction).
 */
int
s3_request_set_headers(S3REQUEST *request, struct curl_slist *headers)
{
	request->headers = headers;
	return 0;
}
