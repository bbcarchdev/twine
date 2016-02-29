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

static char *twine_config_key_alloc_(TWINE *restrict context, const char *restrict name);

/* Internal API: Return the path to the default configuration file used by Twine */
const char *
twine_config_path(void)
{
	return SYSCONFDIR "/twine.conf";
}

/* Public API: Retrieve a configuration key by copying its value into the
 * supplied buffer.
 *
 * @key is formatted as "section:name". As a special case, if section is
 * simply an asterisk, then it is replaced in turn by the appname provided
 * via twine_set_appname() and then "twine".
 */
size_t
twine_config_get(const char *key, const char *defval, char *buf, size_t bufsize)
{
	char *kp;
	size_t r;

	if(!twine_)
	{
		if(!defval)
		{
			defval = "";
		}
		if(buf && bufsize)
		{
			strncpy(buf, defval, bufsize);
			buf[bufsize - 1] = 0;
		}
		return strlen(defval);
	}
	if(!strncmp(key, "*:", 2))
	{
		kp = twine_config_key_alloc_(twine_, key + 2);
		if(!kp)
		{
			return (size_t) -1;
		}
		if(twine_->appname)
		{
			strcpy(kp, twine_->appname);
			kp[twine_->appnamelen] = ':';
			strcpy(&(kp[twine_->appnamelen + 1]), key + 2);
			r = twine_->config.config_get(kp, NULL, buf, bufsize);
			/* Was it found? */
			if(r)
			{
				return r;
			}
		}
		strcpy(kp, DEFAULT_CONFIG_SECTION);
		strcat(kp, key + 2);
		return twine_->config.config_get(kp, defval, buf, bufsize);
	}	
	return twine_->config.config_get(key, defval, buf, bufsize);
}

/* Public API: Retrieve the value of a configuration key, allocating a
 * buffer to hold the result and returning it.
 *
 * The rules governing @key are as described in twine_config_get().
 *
 * It is the caller's responsibility to free the result via free().
 */

char *
twine_config_geta(const char *key, const char *defval)
{
	char *r, *kp;

	if(!twine_)
	{
		return defval ? strdup(defval) : NULL;
	}
	if(!strncmp(key, "*:", 2))
	{
		kp = twine_config_key_alloc_(twine_, key + 2);
		if(!kp)
		{
			return NULL;
		}
		if(twine_->appname)
		{
			strcpy(kp, twine_->appname);
			kp[twine_->appnamelen] = ':';
			strcpy(&(kp[twine_->appnamelen + 1]), key + 2);
			r = twine_->config.config_geta(kp, NULL);
			/* Was it found? */
			if(r)
			{
				return r;
			}
		}
		strcpy(kp, DEFAULT_CONFIG_SECTION);
		strcat(kp, key + 2);
		return twine_->config.config_geta(kp, defval);
	}	

	return twine_->config.config_geta(key, defval);
}

/* Public API: Retrieve the value of a configuration key, parsing and
 * returning it as an integer.
 *
 * The rules governing @key are as described in twine_config_get().
 */
int
twine_config_get_int(const char *key, int defval)
{
	char *kp;

	if(!twine_)
	{
		return defval;
	}
	/* When the "*:name" form is used, we must do this in the opposite
	 * direction to twine_config_get(), as there's no way to determine
	 * whether config_get_int() returned successfully or not.
	 */
	if(!strncmp(key, "*:", 2))
	{
		kp = twine_config_key_alloc_(twine_, key + 2);
		if(!kp)
		{
			return -1;
		}
		/* Fetch the value from the default section first, using the supplied
		 * defval if it's not found, or leaving defval unchanged if not.
		 */
		strcpy(kp, DEFAULT_CONFIG_SECTION);
		strcat(kp, key + 2);
		defval = twine_->config.config_get_int(kp, defval);		
		if(twine_->appname)
		{
			/* Attempt to fetch using the app-specific key, using the
			 * possibly-modified defval (determined above) if not found.
			 */
			strcpy(kp, twine_->appname);
			kp[twine_->appnamelen] = ':';
			strcpy(&(kp[twine_->appnamelen + 1]), key + 2);
			return twine_->config.config_get_int(kp, defval);
		}
		return defval;
	}
	return twine_->config.config_get_int(key, defval);
}

/* Public API: Retrieve the value of a configuration key, parsing and
 * returning it as a boolean value (0 = false, -1 = true).
 *
 * The rules governing @key are as described in twine_config_get().
 */
int
twine_config_get_bool(const char *key, int defval)
{
	char *kp;

	if(!twine_)
	{
		return defval;
	}
	if(!strncmp(key, "*:", 2))
	{
		kp = twine_config_key_alloc_(twine_, key + 2);
		if(!kp)
		{
			return -1;
		}
		/* Fetch the value from the default section first, using the supplied
		 * defval if it's not found, or leaving defval unchanged if not.
		 */
		strcpy(kp, DEFAULT_CONFIG_SECTION);
		strcat(kp, key + 2);
		defval = twine_->config.config_get_bool(kp, defval);
		if(twine_->appname)
		{
			/* Attempt to fetch using the app-specific key, using the
			 * possibly-modified defval (determined above) if not found.
			 */
			strcpy(kp, twine_->appname);
			kp[twine_->appnamelen] = ':';
			strcpy(&(kp[twine_->appnamelen + 1]), key + 2);
			return twine_->config.config_get_bool(kp, defval);
		}
		return defval;
	}
	return twine_->config.config_get_bool(key, defval);
}

/* Public API: retrieve all configuration values from @section and @key,
 * invoking the provided callback for each. Note that a read-lock will be
 * held on the configuration while this call is in progress, meaning that
 * other reads can read from it, but will be blocked from writing to it.
 *
 * If @section is NULL, all keys from all sections will be passed to the
 * callback (the value, if any, of @key will be ignored)
 *
 * If @key is NULL, all keys from @section will be passed to the callback.
 *
 * If @section is "*", the app-name section (if set) will be iterated, and if
 * no keys exist, the default section will then be iterated.
 *
 * @data is passed directly as the last parameter to the callback and is not
 * used by this function itself.
 *
 * The return value is the number of keys passed to the callback, or -1 if
 * an error occurs.
 */
int
twine_config_get_all(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data)
{
	int r;

	if(!twine_)
	{
		return 0;
	}
	if(section && !strcmp(section, "*"))
	{
		if(twine_->appname)
		{
			r = twine_->config.config_get_all(twine_->appname, key, fn, data);
			if(r)
			{
				return r;
			}
		}
		return twine_->config.config_get_all(DEFAULT_CONFIG_SECTION_NAME, key, fn, data);
	}
	return twine_->config.config_get_all(section, key, fn, data);
}

static char *
twine_config_key_alloc_(TWINE *restrict context, const char *restrict name)
{
	size_t l;
	char *p;

	l = DEFAULT_CONFIG_SECTION_LEN;
	if(context->appname)
	{
		if(context->appnamelen >= l)
		{
			l = context->appnamelen + 1;
		}
	}
	l += strlen(name) + 1;
	if(l > context->keybuflen)
	{
		p = (char *) realloc(context->keybuf, l);
		if(!p)
		{
			return NULL;
		}
		context->keybuf = p;
		context->keybuflen = l;
	}
	return context->keybuf;
}
