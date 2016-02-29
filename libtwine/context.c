/* Twine: Internal API
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

static void twine_global_init_(void);

static pthread_once_t twine_global_once_ = PTHREAD_ONCE_INIT;

/* The current Twine context */
TWINE *twine_ = NULL;

/* Internal API: Create a new Twine context
 *
 * If a context already exists, this new one will become the current context
 * until it is destroyed.
 */
TWINE *
twine_create(void)
{
	TWINE *p;

	pthread_once(&twine_global_once_, twine_global_init_);
	p = (TWINE *) calloc(1, sizeof(TWINE));
	if(!p)
	{
		return NULL;
	}
	p->prev = twine_;
	twine_ = p;
	p->logger = vsyslog;
	twine_rdf_init_(p);
	return p;
}

int
twine_destroy(TWINE *context)
{
	TWINE *p;

	/* Un-load plug-ins before removing the context */
	twine_plugin_unload_all_(context);
	/* Remove this context from the chain */
	if(context == twine_)
	{
		twine_ = context->prev;
	}
	else
	{
		for(p = twine_; p; p = p->prev)
		{
			if(p->prev == context)
			{
				p->prev = context->prev;
				break;
			}
		}
	}
	twine_rdf_cleanup_(context);
	free(context->sparql_uri);
	free(context->sparql_query_uri);
	free(context->sparql_update_uri);
	free(context->sparql_data_uri);
	free(context->keybuf);
	free(context->appname);
	free(context);
	return 0;
}

/* Internal API: Set the logging callback used by Twine and plug-ins */
int
twine_set_logger(TWINE *context, twine_log_fn logger)
{
	context->logger = logger;
	return 0;
}

/* Internal API: Set the configuration callbacks used by Twine and plug-ins */
int
twine_set_config(TWINE *restrict context, struct twine_configfn_struct *restrict config)
{
	context->config = *config;
	return 0;
}

/* Internal API: Set the application name, used when retrieving configuration
 * values. This should be the name of the executable (e.g., 'twine-writerd'),
 * rather than a 'friendly' name.
 *
 * See twine_config_get() for a description of how this is used.
 */
int
twine_set_appname(TWINE *restrict context, const char *appname)
{
	char *p;

	p = strdup(appname);
	if(!p)
	{
		return -1;
	}
	free(context->appname);
	context->appname = p;
	context->appnamelen = strlen(p);
	return 0;
}

/* Internal API: The application is ready to begin processing; perform any
 * final initialisation required before this can happen.
 */
int
twine_ready(TWINE *context)
{
	if(twine_plugin_init_(context))
	{
		return -1;
	}
	if(twine_workflow_init_())
	{
		return -1;
	}
	return 0;
}

/* Private: Perform one-time initialisation */
static void
twine_global_init_(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
}

