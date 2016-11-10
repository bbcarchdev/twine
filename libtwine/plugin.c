/* Twine: Plug-in handling
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

#define CALLBACK_BLOCKSIZE              4
#define POSTPROC_BLOCKSIZE              4
#define PLUGINDIR                       LIBDIR "/" PACKAGE_TARNAME "/"

static int twine_plugin_config_cb_(const char *key, const char *value, void *data);

/* Public: register an input handler for a particular MIME type */
int
twine_plugin_add_input(TWINE *context, const char *mimetype, const char *description, TWINEINPUTFN fn, void *userdata)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(context, userdata);
	if(!p)
	{
		return -1;
	}
	p->m.input.type = strdup(mimetype);
	p->m.input.desc = strdup(description);
	if(!p->m.input.type || !p->m.input.desc)
	{
		if (p->m.input.type)
		{
			free(p->m.input.type);
		}
		if (p->m.input.desc)
		{
			free(p->m.input.desc);
		}
		twine_logf(LOG_CRIT, "failed to allocate memory to register input handler for type '%s'\n", mimetype);
		return -1;
	}
	p->m.input.fn = fn;
	p->type = TCB_INPUT;
	twine_logf(LOG_INFO, "registered input handler for type: '%s' (%s)\n", mimetype, description);
	return 0;
}

/* Public: determine whether an input handler for a MIME type has been
 * registered
 */
int
twine_plugin_input_exists(TWINE *restrict context, const char *mimetype)
{
	size_t l;

	for(l = 0; l < context->cbcount; l++)
	{
		if(context->callbacks[l].type == TCB_INPUT &&
		   !strcasecmp(context->callbacks[l].m.input.type, mimetype))
		{
			return 1;
		}
		if(context->callbacks[l].type == TCB_LEGACY_MIME &&
		   !strcasecmp(context->callbacks[l].m.legacy_mime.type, mimetype))
		{
			return 1;
		}
	}
	return 0;	
}

/* Public: register a bulk input handler for a particular MIME type */
int twine_plugin_add_bulk(TWINE *restrict context, const char *restrict mimetype, const char *restrict description, TWINEBULKFN fn, void *userdata)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(context, userdata);
	if(!p)
	{
		return -1;
	}
	p->m.bulk.type = strdup(mimetype);
	p->m.bulk.desc = strdup(description);
	if(!p->m.bulk.type || !p->m.bulk.desc)
	{
		free(p->m.bulk.type);
		free(p->m.bulk.desc);
		twine_logf(LOG_CRIT, "failed to allocate memory to register bulk handler for type '%s'\n", mimetype);
		return -1;
	}
	p->m.bulk.fn = fn;
	p->type = TCB_BULK;
	twine_logf(LOG_INFO, "registered bulk handler for type: '%s' (%s)\n", mimetype, description);
	return 0;
}

/* Public: determine whether a bulk input handler for a particular MIME type
 * has been registered
 */
int
twine_plugin_bulk_exists(TWINE *restrict context, const char *mimetype)
{
	size_t l;

	for(l = 0; l < context->cbcount; l++)
	{
		if(context->callbacks[l].type == TCB_BULK &&
		   !strcasecmp(context->callbacks[l].m.bulk.type, mimetype))
		{
			return 1;
		}
		if(context->callbacks[l].type == TCB_LEGACY_BULK &&
		   !strcasecmp(context->callbacks[l].m.legacy_bulk.type, mimetype))
		{
			return 1;
		}
	}
	return 0;
}

/* Public: register a graph processor */
int
twine_plugin_add_processor(TWINE *context, const char *name, TWINEPROCESSORFN fn, void *userdata)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(context, userdata);
	if(!p)
	{
		return -1;
	}
	p->m.processor.name = strdup(name);
	if(!p->m.processor.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register graph processor '%s'\n", name);
		return -1;
	}
	p->m.processor.fn = fn;
	p->type = TCB_PROCESSOR;
	twine_logf(LOG_INFO, "registered graph processor: '%s'\n", name);
	return 0;
}

/* Public: determine whether a particular named graph processor has been
 * registered
 */
int
twine_plugin_processor_exists(TWINE *restrict context, const char *restrict name)
{
	size_t l;

	for(l = 0; l < context->cbcount; l++)
	{
		if(context->callbacks[l].type == TCB_PROCESSOR &&
		   !strcasecmp(context->callbacks[l].m.processor.name, name))
		{
			return 1;
		}
		if(context->callbacks[l].type == TCB_LEGACY_GRAPH &&
		   !strcasecmp(context->callbacks[l].m.legacy_graph.name, name))
		{
			return 1;
		}
	}
	return 0;
}

/* Public: register an update handler */
int twine_plugin_add_update(TWINE *restrict context, const char *restrict name, TWINEUPDATEFN fn, void *userdata)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(context, userdata);
	if(!p)
	{
		return -1;
	}
	p->m.update.name = strdup(name);
	if(!p->m.update.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register update handler '%s'\n", name);
		return -1;
	}
	p->m.update.fn = fn;
	p->type = TCB_UPDATE;
	twine_logf(LOG_INFO, "registered update handler: '%s'\n", name);
	return 0;	
}

/* Public: determine whether a particular named update handler has been
 * registered
 */
int
twine_plugin_update_exists(TWINE *restrict context, const char *restrict name)
{
	size_t l;

	for(l = 0; l < context->cbcount; l++)
	{
		if(context->callbacks[l].type == TCB_LEGACY_UPDATE &&
		   !strcasecmp(context->callbacks[l].m.legacy_update.name, name))
		{
			return 1;
		}
	}
	return 0;
}

/* Internal API: load a plug-in and invoke its initialiser callback */
void *
twine_plugin_load(TWINE *restrict context, const char *restrict pathname)
{
	void *handle;
	TWINEENTRYFN entry;
	twine_plugin_init_fn fn;
	char *fnbuf;
	size_t len;
	TWINE *prevtwine;
	int r;

	twine_logf(LOG_DEBUG, "loading plug-in %s\n", pathname);
	if(strchr(pathname, '/'))
	{
		fnbuf = NULL;
	}
	else
	{
		len = strlen(pathname) + strlen(PLUGINDIR) + 1;
		fnbuf = (char *) malloc(len);
		if(!fnbuf)
		{
			twine_logf(LOG_CRIT, "failed to allocate %u bytes\n", (unsigned) len);
			return NULL;
		}
		strcpy(fnbuf, PLUGINDIR);
		strcat(fnbuf, pathname);
		pathname = fnbuf;
	}
	prevtwine = twine_;
	twine_ = context;
	handle = dlopen(pathname, RTLD_NOW);
	if(!handle)
	{
		twine_logf(LOG_ERR, "failed to load %s: %s\n", pathname, dlerror());
		free(fnbuf);
		twine_ = prevtwine;
		return NULL;
	}
	entry = (TWINEENTRYFN) dlsym(handle, "twine_entry");
	fn = NULL;
	if(!entry)
	{
		fn = (twine_plugin_init_fn) dlsym(handle, "twine_plugin_init");
		if(!fn)
		{
			twine_logf(LOG_ERR, "%s is not a Twine plug-in\n", pathname);
			dlclose(handle);
			free(fnbuf);
			twine_ = prevtwine;
			errno = EINVAL;
			return NULL;
		}
	}
	context->plugin_current = handle;
	if(entry)
	{
		r = entry(context, TWINE_ATTACHED, handle);
	}
	else
	{
		twine_logf(LOG_WARNING, "plug-in '%s' uses deprecated APIs\n", pathname);
		r = fn();
	}
	if(r)
	{
		twine_logf(LOG_ERR, "initialisation of plug-in %s failed\n", pathname);
		context->plugin_current = NULL;
		twine_ = prevtwine;
		twine_plugin_unload(context, handle);		
		free(fnbuf);
		return NULL;
	}
	twine_logf(LOG_DEBUG, "loaded plug-in %s\n", pathname);
	free(fnbuf);
	context->plugin_current = NULL;
	twine_ = prevtwine;
	return handle;
}

/* Internal API: de-register all plugins attached to a module and un-load
 * the plug-in
 */
int
twine_plugin_unload(TWINE *restrict context, void *handle)
{
	size_t l;
	TWINEENTRYFN entry;
	twine_plugin_cleanup_fn fn;
	void *prev;

	l = 0;
	while(l < context->cbcount)
	{
		if(context->callbacks[l].module != handle)
		{
			l++;
			continue;
		}
		switch(context->callbacks[l].type)
		{
		case TCB_NONE:
			break;
		case TCB_INPUT:
			free(context->callbacks[l].m.input.type);
			free(context->callbacks[l].m.input.desc);
			break;
		case TCB_BULK:
			free(context->callbacks[l].m.bulk.type);
			free(context->callbacks[l].m.bulk.desc);
			break;
		case TCB_UPDATE:
			break;
		case TCB_PROCESSOR:
			free(context->callbacks[l].m.processor.name);
			break;
		case TCB_LEGACY_MIME:
			free(context->callbacks[l].m.legacy_mime.type);
			free(context->callbacks[l].m.legacy_mime.desc);
			break;
		case TCB_LEGACY_BULK:
			free(context->callbacks[l].m.legacy_bulk.type);
			free(context->callbacks[l].m.legacy_bulk.desc);
			break;
		case TCB_LEGACY_UPDATE:
			free(context->callbacks[l].m.legacy_update.name);
			break;
		case TCB_LEGACY_GRAPH:
			free(context->callbacks[l].m.legacy_graph.name);
			break;
		}
		if(l + 1 < context->cbcount)
		{
			memmove(&(context->callbacks[l]), &(context->callbacks[l + 1]), sizeof(struct twine_callback_struct) * (context->cbcount - l - 1));
		}
		context->cbcount--;
	}
	if(handle)
	{
		entry = (TWINEENTRYFN) dlsym(handle, "twine_entry");
		if(entry)
		{
			prev = context->plugin_current;
			context->plugin_current = handle;
			entry(context, TWINE_DETACHED, handle);
			context->plugin_current = prev;
		}
		else
		{
			fn = (twine_plugin_cleanup_fn) dlsym(handle, "twine_plugin_done");
			if(fn)
			{
				prev = context->plugin_current;
				context->plugin_current = handle;
				fn();
				context->plugin_current = prev;
			}
		}
		dlclose(handle);
	}
	return 0;
}

/* Private: temporarily enable or disable internal registration of plug-ins */
int
twine_plugin_allow_internal_(TWINE *context, int enable)
{
	context->allow_internal = enable;
	return 0;
}

/* Private: unload all plug-ins attached to a context */
int
twine_plugin_unload_all_(TWINE *context)
{
	void *handle;
	size_t c, n;

	n = 0;
	for(c = 0; c < context->cbcount;)
	{
		n++;
		handle = context->callbacks[c].module;
		twine_plugin_unload(context, handle);
		if(c < context->cbcount && context->callbacks[c].module == handle)
		{
			twine_logf(LOG_ERR, "failed to unregister context->callbacks for handle 0x%08x; aborting clean-up\n", (unsigned long) handle);
			return -1;
		}
	}
	if(!context->cbcount)
	{		
		free(context->callbacks);
		context->callbacks = NULL;
		context->cbcount = 0;
		context->cbsize = 0;
	} 
	if(context->plugins_enabled || n)
	{
		twine_logf(LOG_DEBUG, "all plug-ins unregistered\n");
	}
	return 0;
}

/* Private: add a new callback */
struct twine_callback_struct *
twine_plugin_callback_add_(TWINE *restrict context, void *restrict data)
{
	struct twine_callback_struct *p;

	if(!context->plugin_current && !context->allow_internal)
	{
		twine_logf(LOG_ERR, "attempt to register a new callback outside of a module\n");
		return NULL;
	}
	if(context->cbcount >= context->cbsize)
	{
		p = (struct twine_callback_struct *) realloc(context->callbacks, sizeof(struct twine_callback_struct) * (context->cbsize + CALLBACK_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, "failed to allocate memory to register callback\n");
			return NULL;
		}
		context->callbacks = p;
		context->cbsize += CALLBACK_BLOCKSIZE;
	}
	p = &(context->callbacks[context->cbcount]);
	memset(p, 0, sizeof(struct twine_callback_struct));
	p->module = context->plugin_current;
	p->data = data;
	context->cbcount++;
	return p;
}

/* Private: Load all configured plug-ins into a context */
int
twine_plugin_init_(TWINE *context)
{
	int r;

	if(!context->plugins_enabled)
	{
		return 0;
	}
	r = twine_config_get_all("plugins", "module", twine_plugin_config_cb_, context);
	if(r < 0)
	{
		return -1;
	}
	else if(r)
	{
		if(context->appname && strcmp(context->appname, DEFAULT_CONFIG_SECTION_NAME))
		{
			twine_logf(LOG_NOTICE, "The [plugins] configuration section has been deprecated; you should use plugin=name.so in the common [%s] section or application-specific [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME, context->appname);
		}
		else
		{
			twine_logf(LOG_NOTICE, "The [plugins] configuration section has been deprecated; you should use plugin=name.so in the common [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME);
		}
		return 0;
	}
	if(twine_config_get_all("*", "plugin", twine_plugin_config_cb_, context) < 0)
	{
		return -1;
	}
	return 0;
}

/* Private: callback invoked for each plugin=NAME.so which appears in the
 * configuration
 */
static int
twine_plugin_config_cb_(const char *key, const char *value, void *data)
{
	TWINE *context;

	(void) key;

	context = (TWINE *) data;
	if(twine_plugin_load(context, value) == NULL)
	{
		return -1;
	}
	return 0;
}
