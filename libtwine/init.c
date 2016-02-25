/* Twine: Internal API
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

#include "p_libtwine.h"

int
twine_init_(twine_log_fn logger)
{
	curl_global_init(CURL_GLOBAL_ALL);
	twine_log_init_(logger);
	twine_rdf_init_();
	twine_workflow_init_();
	return 0;
}

int
twine_cleanup_(void)
{
	twine_log_cleanup_();
	twine_plugin_unload_all_();
	twine_rdf_cleanup_();
	curl_global_cleanup();
	return 0;
}
