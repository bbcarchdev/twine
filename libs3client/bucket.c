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

/* Create an object representing an S3 bucket */
S3BUCKET *
s3_create(const char *bucket)
{
	S3BUCKET *p;

	p = (S3BUCKET *) calloc(1, sizeof(S3BUCKET));
	if(!p)
	{
		return NULL;
	}
	p->bucket = strdup(bucket);
	p->endpoint = strdup(S3_DEFAULT_ENDPOINT);
	if(!p->bucket || !p->endpoint)
	{
		s3_destroy(p);
		return NULL;
	}
	return p;
}

/* Free the resources used by a bucket */
int
s3_destroy(S3BUCKET *bucket)
{
	if(!bucket)
	{
		errno = EINVAL;
		return -1;
	}
	free(bucket->bucket);
	free(bucket->access);
	free(bucket->secret);	
	free(bucket->endpoint);
	free(bucket->basepath);
	free(bucket);
	return 0;
}

/* Set the name of the S3 bucket */
int
s3_set_bucket(S3BUCKET *bucket, const char *name)
{
	char *p;

	p = strdup(name);
	if(!p)
	{
		return -1;
	}
	free(bucket->bucket);
	bucket->bucket = p;
	return 0;
}

/* Set the access key to be used in requests for this bucket */
int
s3_set_access(S3BUCKET *bucket, const char *key)
{
	char *p;

	p = strdup(key);
	if(!p)
	{
		return -1;
	}
	free(bucket->access);
	bucket->access = p;
	return 0;
}

/* Set the secret to be used in requests for this bucket */
int
s3_set_secret(S3BUCKET *bucket, const char *key)
{
	char *p;

	p = strdup(key);
	if(!p)
	{
		return -1;
	}
	free(bucket->secret);
	bucket->secret = p;
	return 0;
}

/* Set the endpoint to be used (in place of s3.amazonaws.com) */
int
s3_set_endpoint(S3BUCKET *bucket, const char *host)
{
	char *p;

	p = strdup(host);
	if(!p)
	{
		return -1;
	}
	free(bucket->endpoint);
	bucket->endpoint = p;
	return 0;
}

/* Set the base path to be used in future requests */
int
s3_set_basepath(S3BUCKET *bucket, const char *path)
{
	char *p;

	p = strdup(path);
	if(!p)
	{
		return -1;
	}
	free(bucket->basepath);
	bucket->basepath = p;
	return 0;
}
