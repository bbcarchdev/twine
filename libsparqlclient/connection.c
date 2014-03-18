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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libsparqlclient.h"

SPARQL *
sparql_create(void)
{
	SPARQL *p;
	
	p = (SPARQL *) calloc(1, sizeof(SPARQL));
	if(!p)
	{
		return NULL;
	}
	
	return p;
}

int
sparql_destroy(SPARQL *connection)
{
	free(connection->query_uri);
	free(connection->update_uri);
	free(connection->data_uri);
	free(connection);
	return 0;
}

int
sparql_set_query_uri(SPARQL *connection, const char *uri)
{
	char *p;
	
	p = strdup(uri);
	if(!p)
	{
		return -1;
	}
	free(connection->query_uri);
	connection->query_uri = p;
	return 0;
}

int
sparql_set_data_uri(SPARQL *connection, const char *uri)
{
	char *p;
	
	p = strdup(uri);
	if(!p)
	{
		return -1;
	}
	free(connection->data_uri);
	connection->data_uri = p;
	return 0;
}

int
sparql_set_update_uri(SPARQL *connection, const char *uri)
{
	char *p;
	
	p = strdup(uri);
	if(!p)
	{
		return -1;
	}
	free(connection->update_uri);
	connection->update_uri = p;
	return 0;
}

int sparql_set_logger(SPARQL *connection, sparql_logger_fn logger)
{
	connection->logger = logger;
	return 0;
}

int
sparql_set_verbose(SPARQL *connection, int verbose)
{
	connection->verbose = verbose;
	return 0;
}

void
sparql_logf_(SPARQL *connection, int priority, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	if(connection->logger)
	{
		connection->logger(priority, format, ap);
	}
}
