/* Twine: Plug-in interface -- internal API
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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

#ifndef LIBTWINE_INTERNAL_H_
# define LIBTWINE_INTERNAL_H_           1

# include "libtwine.h"

typedef void (*twine_log_fn)(int prio, const char *fmt, va_list ap);

struct twine_configfn_struct
{
	size_t (*config_get)(const char *key, const char *defval, char *buf, size_t bufsize);
	char *(*config_geta)(const char *key, const char *defval);
	int (*config_get_int)(const char *key, int defval);
	int (*config_get_bool)(const char *key, int defval);
	int (*config_get_all)(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);
};

int twine_init_(twine_log_fn logger);
int twine_cleanup_(void);
int twine_config_init_(struct twine_configfn_struct *fns);
int twine_sparql_defaults_(const char *query_uri, const char *update_uri, const char *data_uri, int verbose);
int twine_plugin_load_(const char *pathname);
int twine_plugin_unregister_all_(void *handle);
int twine_preproc_registered_(void);
int twine_postproc_registered_(void);
int twine_preproc_process_(twine_graph *graph);
int twine_postproc_process_(twine_graph *graph);

#endif /*!LIBTWINE_INTERNAL_H_*/
