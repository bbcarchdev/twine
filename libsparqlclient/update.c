/* Twine: SPARQL updates
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

#include "p_libsparqlclient.h"

int
sparql_update(SPARQL *connection, const char *statement, size_t length)
{
	CURL *ch;
	char *buf;
	size_t buflen;

	ch = curl_easy_init();
	if(!ch)
	{
		sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to initialise cURL handle\n");
		return -1;
	}
	buflen = sparql_urlencode_lsize_(statement, length);
	buf = (char *) malloc(16 + buflen);
	if(!buf)
	{
		sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to allocate %u bytes\n", (unsigned) length + 16);
		curl_easy_cleanup(ch);
		return -1;
	}
	strcpy(buf, "update=");
	sparql_urlencode_l_(statement, length, buf + 7, buflen);
	sparql_logf_(connection, LOG_DEBUG, "SPARQL: performing SPARQL update to %s\n", connection->update_uri);
	sparql_logf_(connection, LOG_DEBUG, "SPARQL: POST data: %s\n", buf);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, connection->verbose);
	curl_easy_setopt(ch, CURLOPT_URL, connection->update_uri);	
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, buf);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, length + 7);
	curl_easy_perform(ch);
	curl_easy_cleanup(ch);
	free(buf);

	return 0;
}
