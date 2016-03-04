/* Twine: SPARQL client helpers
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

#include "p_libtwine.h"

/* Internal API: set configuration for SPARQL connections
 *
 * Note that this will have no effect on SPARQL connection objects which
 * have been created prior to this call.
 */
int
twine_set_sparql(TWINE *restrict context, const char *base_uri, const char *query_uri, const char *update_uri, const char *data_uri, int verbose)
{
	if(base_uri)
	{
		context->sparql_uri = strdup(base_uri);
		if(!context->sparql_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL endpoint is <%s>\n", context->sparql_uri);
	}
	if(query_uri)
	{
		context->sparql_query_uri = strdup(query_uri);
		if(!context->sparql_query_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL query endpoint is <%s>\n", context->sparql_query_uri);
	}
	if(update_uri)
	{
		context->sparql_update_uri = strdup(update_uri);
		if(!context->sparql_update_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL update endpoint is <%s>\n", context->sparql_update_uri);
	}
	if(data_uri)
	{
		context->sparql_data_uri = strdup(data_uri);
		if(!context->sparql_data_uri)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, "SPARQL RESTful endpoint is <%s>\n", context->sparql_data_uri);
	}
	context->sparql_debug = !!(verbose);
	return 0;
}

/* Public: Create a new SPARQL connection */
SPARQL *
twine_sparql_create(void)
{
	SPARQL *p;

	p = sparql_create(twine_->sparql_uri);
	if(!p)
	{
		twine_logf(LOG_CRIT, "failed to create new SPARQL connection\n");
		return NULL;
	}
	sparql_set_logger(p, twine_->logger);
	sparql_set_verbose(p, twine_->sparql_debug);
	if(twine_->sparql_query_uri)
	{
		sparql_set_query_uri(p, twine_->sparql_query_uri);
	}
	if(twine_->sparql_update_uri)
	{
		sparql_set_update_uri(p, twine_->sparql_update_uri);
	}
	if(twine_->sparql_data_uri)
	{
		sparql_set_data_uri(p, twine_->sparql_data_uri);
	}
	return p;
}

/* Private: Initialise the SPARQL connection details for a context */
int
twine_sparql_init_(TWINE *context)
{
	if(context->sparql_uri ||
	   (context->sparql_query_uri && context->sparql_update_uri && context->sparql_data_uri))
	{
		/* The SPARQL connection parameters have already been specified */
		if(context->sparql_debug == -2)
		{
			context->sparql_debug = 0;
		}
		return 0;
	}
	/* First, look for parameters in the [sparql] section, for compatibility
	 * with older versions of Twine
	 */
	context->sparql_debug = twine_config_get_bool("sparql:verbose", context->sparql_debug);
	context->sparql_uri = twine_config_geta("sparql:uri", NULL);
	context->sparql_query_uri = twine_config_geta("sparql:query", NULL);
	context->sparql_update_uri = twine_config_geta("sparql:update", NULL);
	context->sparql_data_uri = twine_config_geta("sparql:data", NULL);
	if(context->sparql_uri || context->sparql_query_uri ||
	   context->sparql_update_uri || context->sparql_data_uri)
	{
		if(context->appname && strcmp(context->appname, DEFAULT_CONFIG_SECTION_NAME))
		{
			twine_logf(LOG_NOTICE, "The [sparql] configuration section has been deprecated; you should use sparql=URI, sparql-verbose=yes|no, sparql-query=URI, sparql-update=URI, and sparql-data=URI in the common [%s] section or the application-specific [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME, context->appname);
		}
		else
		{
			twine_logf(LOG_NOTICE, "The [sparql] configuration section has been deprecated; you should use sparql=URI, sparql-verbose=yes|no, sparql-query=URI, sparql-update=URI, and sparql-data=URI in the common [%s] section\n", DEFAULT_CONFIG_SECTION_NAME, context->appname);
		}
		if(context->sparql_debug == -2)
		{
			context->sparql_debug = 0;
		}
		return 0;
	}
	/* Now obtain the values normally */
	context->sparql_debug = twine_config_get_bool("*:sparql-verbose", 0);
	context->sparql_uri = twine_config_geta("*:sparql", "http://localhost/");
	context->sparql_query_uri = twine_config_geta("*:sparql-query", NULL);
	context->sparql_update_uri = twine_config_geta("*:sparql-update", NULL);
	context->sparql_data_uri = twine_config_geta("*:sparql-data", NULL);	
	return 0;
}
