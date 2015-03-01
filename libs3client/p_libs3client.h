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

#ifndef P_LIBS3CLIENT_H_
# define P_LIBS3CLIENT_H_               1

# define _BSD_SOURCE                    1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <errno.h>
# include <ctype.h>

# include <curl/curl.h>
# include <openssl/hmac.h>
# include <openssl/evp.h>
# include <openssl/bio.h>
# include <openssl/buffer.h>

# include "libs3client.h"

# define S3_DEFAULT_ENDPOINT            "s3.amazonaws.com"

struct s3_bucket_struct
{
	char *bucket;
	char *access;
	char *secret;
	char *endpoint;
	char *basepath;
};

struct s3_request_struct
{
	S3BUCKET *bucket;
	char *resource;
	char *method;
	CURL *ch;
	struct curl_slist *headers;
	int finalised;
};

#endif /*!P_LIBS3CLIENT_H_*/
