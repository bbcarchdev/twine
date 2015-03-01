/* Twine: Internal API
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libtwine.h"

static struct twine_configfn_struct configfn;

int
twine_config_init_(struct twine_configfn_struct *fns)
{
	configfn = *fns;
	return 0;
}

size_t
twine_config_get(const char *key, const char *defval, char *buf, size_t bufsize)
{
	return configfn.config_get(key, defval, buf, bufsize);
}

char *
twine_config_geta(const char *key, const char *defval)
{
	return configfn.config_geta(key, defval);
}

int
twine_config_get_int(const char *key, int defval)
{
	return configfn.config_get_int(key, defval);
}

int
twine_config_get_bool(const char *key, int defval)
{
	return configfn.config_get_bool(key, defval);
}

int
twine_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data)
{
	return configfn.config_get_all(section, key, fn, data);
}
