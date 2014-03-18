/* Twine: SPARQL client
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

#ifndef P_LIBSPARQLCLIENT_H_
# define P_LIBSPARQLCLIENT_H_           1

# include <stddef.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <syslog.h>
# include <curl/curl.h>

# include "libsparqlclient.h"

struct sparql_connection_struct
{
	char *query_uri;
	char *update_uri;
	char *data_uri;
	int verbose;
	sparql_logger_fn logger;
};

size_t sparql_urlencode_size_(const char *src);
size_t sparql_urlencode_lsize_(const char *src, size_t srclen);
int sparql_urlencode_(const char *src, char *dest, size_t destlen);
int sparql_urlencode_l_(const char *src, size_t srclen, char *dest, size_t destlen);
void sparql_logf_(SPARQL *connection, int priority, const char *format, ...);

#endif /*!P_LIBSPARQLCLIENT_H_*/
