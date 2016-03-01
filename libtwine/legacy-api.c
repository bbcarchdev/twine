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

int
twine_init_(TWINELOGFN logger)
{
	TWINE *p;

	p = twine_create();
	if(!p)
	{
		return -1;
	}
	twine_set_logger(p, logger);
	return 0;
}

int
twine_cleanup_(void)
{
	if(twine_)
	{
		twine_destroy(twine_);
	}
	return 0;
}

int
twine_preflight_(void)
{
	return twine_ready(twine_);
}

int
twine_config_init_(TWINECONFIGFNS *fns)
{
	twine_set_config(twine_, fns);
	return 0;
}

int
twine_plugin_load_(const char *pathname)
{
	void *h;

	h = twine_plugin_load(twine_, pathname);
	if(h)
	{
		return 0;
	}
	return -1;
}

int
twine_plugin_unregister_all_(void *handle)
{
	return twine_plugin_unload(twine_, handle);
}

int
twine_sparql_defaults_(const char *base_uri, const char *query_uri, const char *update_uri, const char *data_uri, int verbose)
{
	return twine_set_sparql(twine_, base_uri, query_uri, update_uri, data_uri, verbose);
}
