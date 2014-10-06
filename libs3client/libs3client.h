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

#ifndef LIBS3CLIENT_H_
# define LIBS3CLIENT_H_                 1

# include <curl/curl.h>

typedef struct s3_bucket_struct S3BUCKET;
typedef struct s3_request_struct S3REQUEST;

/* Create an object representing an S3 bucket */
S3BUCKET *s3_create(const char *bucket);

/* Free the resources used by a bucket */
int s3_destroy(S3BUCKET *bucket);

/* Set the name of the S3 bucket */
int s3_set_bucket(S3BUCKET *bucket, const char *name);

/* Set the access key to be used in requests for this bucket */
int s3_set_access(S3BUCKET *bucket, const char *key);

/* Set the secret to be used in requests for this bucket */
int s3_set_secret(S3BUCKET *bucket, const char *key);

/* Set the endpoint to be used (in place of s3.amazonaws.com) */
int s3_set_endpoint(S3BUCKET *bucket, const char *host);

/* Set the base path to be used for all requests to this bucket */
int s3_set_basepath(S3BUCKET *bucket, const char *path);

/* Create a new request for a resource within a bucket */
S3REQUEST *s3_request_create(S3BUCKET *bucket, const char *resource, const char *method);

/* Destroy a request */
int s3_request_destroy(S3REQUEST *request);

/* Finalise (sign) a request */
int s3_request_finalise(S3REQUEST *request);

/* Perform a request, finalising if needed */
int s3_request_perform(S3REQUEST *request);

/* Obtain (creating if needed) the cURL handle for this request */
CURL *s3_request_curl(S3REQUEST *request);

/* Obtain the headers list for this request */
struct curl_slist *s3_request_headers(S3REQUEST *request);

/* Set the headers list for this request (the list will be freed upon
 * request destruction).
 */
int s3_request_set_headers(S3REQUEST *request, struct curl_slist *headers);

/* Sign an AWS request, appending a suitable 'Authorization: AWS ...' header
 * to the list provided.
 */
struct curl_slist *s3_sign(const char *method, const char *resource, const char *access_key, const char *secret, struct curl_slist *headers);

#endif /*!LIBS3CLIENT_H_*/
