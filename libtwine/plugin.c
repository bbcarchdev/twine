/* Twine: Plug-in handling
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

#define CALLBACK_BLOCKSIZE              4
#define POSTPROC_BLOCKSIZE              4
#define PLUGINDIR                       LIBDIR "/" PACKAGE_TARNAME "/"

static void *current;

static struct twine_callback_struct *callbacks;
static size_t cbcount, cbsize;
static int internal;

static struct twine_callback_struct *twine_plugin_callback_add_(void *data);
static int twine_plugin_config_cb_(const char *key, const char *value, void *data);

/* Internal API: load a plug-in and invoke its initialiser callback */
void *
twine_plugin_load(TWINE *restrict context, const char *restrict pathname)
{
	void *handle;
	twine_plugin_init_fn fn;
	char *fnbuf;
	size_t len;
	TWINE *prevtwine;

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
	twine_logf(LOG_DEBUG, "invoking plug-in initialisation function for %s\n", pathname);
	current = handle;
	if(fn())
	{
		twine_logf(LOG_ERR, "initialisation of plug-in %s failed\n", pathname);
		current = NULL;
		twine_ = prevtwine;
		twine_plugin_unload(context, handle);		
		free(fnbuf);
		return NULL;
	}
	twine_logf(LOG_INFO, "loaded plug-in %s\n", pathname);
	free(fnbuf);
	current = NULL;
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
	twine_plugin_cleanup_fn fn;

	/* We don't actually use the context, because there's a single shared
	 * list of callbacks which we can iterate.
	 */
	(void) context;

	l = 0;
	while(l < cbcount)
	{
		if(callbacks[l].module != handle)
		{
			l++;
			continue;
		}
		switch(callbacks[l].type)
		{
		case TCB_NONE:
			break;
		case TCB_MIME:
			free(callbacks[l].m.mime.type);
			free(callbacks[l].m.mime.desc);
			break;
		case TCB_BULK:
			free(callbacks[l].m.bulk.type);
			free(callbacks[l].m.bulk.desc);
			break;
		case TCB_UPDATE:
			free(callbacks[l].m.update.name);
			break;
		case TCB_GRAPH:
			free(callbacks[l].m.graph.name);
			break;
		}
		if(l + 1 < cbcount)
		{
			memmove(&(callbacks[l]), &(callbacks[l + 1]), sizeof(struct twine_callback_struct) * (cbcount - l - 1));
		}
		cbcount--;
	}
	if(handle)
	{
		fn = (twine_plugin_cleanup_fn) dlsym(handle, "twine_plugin_done");
		if(fn)
		{
			current = handle;
			fn();
			current = NULL;
		}	   
		dlclose(handle);
	}
	return 0;
}


/* Private: temporarily enable or disable internal registration of plug-ins */
int
twine_plugin_internal_(int enable)
{
	internal = enable;
	return 0;
}

/* Private: unload all plug-ins attached to a context */
int
twine_plugin_unload_all_(TWINE *context)
{
	void *handle;
	size_t c;

	for(c = 0; c < cbcount;)
	{
		if(callbacks[0].context != context)
		{
			c++;
			continue;
		}
		handle = callbacks[c].module;
		twine_plugin_unload(context, handle);
		if(c < cbcount && callbacks[c].module == handle)
		{
			twine_logf(LOG_ERR, "failed to unregister callbacks for handle 0x%08x; aborting clean-up\n", (unsigned long) handle);
			return -1;
		}
	}
	if(!cbcount)
	{		
		free(callbacks);
		callbacks = NULL;
		cbcount = 0;
		cbsize = 0;
	} 
	twine_logf(LOG_INFO, "all plug-ins unregistered\n");
	return 0;
}

/* Public: register a MIME type */
int
twine_plugin_register(const char *mimetype, const char *description, twine_processor_fn fn, void *data)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(data);
	if(!p)
	{
		return -1;
	}
	p->m.mime.type = strdup(mimetype);
	p->m.mime.desc = strdup(description);
	if(!p->m.mime.type || !p->m.mime.desc)
	{
		free(p->m.mime.type);
		free(p->m.mime.desc);
		twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
		return -1;
	}
	p->m.mime.fn = fn;
	p->type = TCB_MIME;
	twine_logf(LOG_INFO, "registered MIME type: '%s' (%s)\n", mimetype, description);
	return 0;
}

/* Public: register a bulk processor for a MIME type */
int
twine_bulk_register(const char *mimetype, const char *description, twine_bulk_fn fn, void *data)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(data);
	if(!p)
	{
		return -1;
	}
	p->m.bulk.type = strdup(mimetype);
	p->m.bulk.desc = strdup(description);
	if(!p->m.bulk.type || !p->m.bulk.desc)
	{
		p->type = TCB_NONE;
		free(p->m.bulk.type);
		free(p->m.bulk.desc);
		twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
		return -1;
	}
	p->m.bulk.fn = fn;
	p->type = TCB_BULK;
	twine_logf(LOG_INFO, "registered bulk processor for MIME type: '%s' (%s)\n", mimetype, description);
	return 0;
}

/* Public: Register a graph processing handler */
int
twine_graph_register(const char *name, twine_graph_fn fn, void *data)
{
	struct twine_callback_struct *g;

	g = twine_plugin_callback_add_(data);
	if(!g)
	{
		return -1;
	}
	g->m.graph.name = strdup(name);
	if(!g->m.graph.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register graph processor\n");
		return -1;
	}
	g->m.graph.fn = fn;
	g->type = TCB_GRAPH;
	twine_logf(LOG_INFO, "registered graph processor: '%s'\n", name);
	return 0;
}

/* Deprecated: register a post-processing handler */
int
twine_postproc_register(const char *name, twine_postproc_fn fn, void *data)
{
	struct twine_callback_struct *g;

	g = twine_plugin_callback_add_(data);
	if(!g)
	{
		return -1;
	}
	g->m.graph.name = (char *) calloc(1, strlen(name) + 6);
	if(!g->m.graph.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register post-processor\n");
		return -1;
	}
	strcpy(g->m.graph.name, "post:");
	strcpy(&(g->m.graph.name[5]), name);
	g->m.graph.fn = fn;
	g->type = TCB_GRAPH;

	twine_logf(LOG_INFO, "registered graph processor: 'post:%s'\n", name);
	return 0;
}

/* Deprecated: register a pre-processing handler */
int
twine_preproc_register(const char *name, twine_preproc_fn fn, void *data)
{
	struct twine_callback_struct *g;

	g = twine_plugin_callback_add_(data);
	if(!g)
	{
		return -1;
	}
	g->m.graph.name = (char *) calloc(1, strlen(name) + 5);
	if(!g->m.graph.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register pre-processor\n");
		return -1;
	}
	strcpy(g->m.graph.name, "pre:");
	strcpy(&(g->m.graph.name[4]), name);
	g->m.graph.fn = fn;
	g->type = TCB_GRAPH;

	twine_logf(LOG_INFO, "registered graph processor: 'pre:%s'\n", name);
	return 0;
}

/* Public: Register an update handler */
int
twine_update_register(const char *name, twine_update_fn fn, void *data)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(data);
	if(!p)
	{
		return -1;
	}
	p->m.update.name = strdup(name);
	if(!p->m.update.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register update handler\n");
		return -1;
	}
	p->m.update.fn = fn;
	p->type = TCB_UPDATE;
	twine_logf(LOG_INFO, "registered update handler: '%s'\n", name);
	return 0;
}

/* Public: Forward a message to a plug-in for processing */
int
twine_plugin_process(const char *mimetype, const unsigned char *message, size_t msglen, const char *subject)
{
	size_t l, tl;
	const char *s;
	void *prev;
	int r;

	(void) subject;
	
	s = strchr(mimetype, ';');
	if(s)
	{
		tl = s - mimetype;
	}
	else
	{
		tl = strlen(mimetype);
	}
	prev = current;
	for(l = 0; l < cbcount; l++)
	{
		if(callbacks[l].type != TCB_MIME)
		{
			continue;
		}
		twine_logf(LOG_DEBUG, "found MIME processor for '%s'\n", callbacks[l].m.mime.type);
		if(!strncasecmp(callbacks[l].m.mime.type, mimetype, tl) && !callbacks[l].m.mime.type[tl])
		{
			current = callbacks[l].module;
			r = callbacks[l].m.mime.fn(mimetype, message, msglen, callbacks[l].data);
			current = prev;
			return r;
		}
	}
	twine_logf(LOG_ERR, "no available processor for messages of type '%s'\n", mimetype);
	return -1;
}

/* Public: Check whether a MIME type is supported by any processor plugin */
int
twine_plugin_supported(const char *mimetype)
{
	size_t l;

	for(l = 0; l < cbcount; l++)
	{
		if(callbacks[l].type != TCB_MIME)
		{
			continue;
		}
		twine_logf(LOG_DEBUG, "found MIME processor for '%s'\n", callbacks[l].m.mime.type);
		if(!strcasecmp(callbacks[l].m.mime.type, mimetype))
		{
			return 1;
		}
	}
	return 0;
}

/* Public: Check whether a MIME type is supported by any bulk processor */
int
twine_bulk_supported(const char *mimetype)
{
	size_t l;

	for(l = 0; l < cbcount; l++)
	{
		if(callbacks[l].type != TCB_BULK)
		{
			continue;
		}
		if(!strcasecmp(callbacks[l].m.bulk.type, mimetype))
		{
			return 1;
		}
	}
	return 0;
}

/* Public: Check whether a plug-in name is recognised as an update handler */
int
twine_update_supported(const char *name)
{
	size_t l;

	for(l = 0; l < cbcount; l++)
	{
		if(callbacks[l].type != TCB_UPDATE)
		{
			continue;
		}
		if(!strcasecmp(callbacks[l].m.update.name, name))
		{
			return 1;
		}
	}
	return 0;
}

/* Public: Check whether a plug-in name is recognised as an graph processing
 * handler */
int
twine_graph_supported(const char *name)
{
	size_t l;

	for(l = 0; l < cbcount; l++)
	{
		if(callbacks[l].type != TCB_GRAPH)
		{
			continue;
		}
		if(!strcasecmp(callbacks[l].m.graph.name, name))
		{
			return 1;
		}
	}
	return 0;
}

/* Public: Perform a bulk import from a file */
int
twine_bulk_import(const char *mimetype, FILE *file)
{
	struct twine_callback_struct *importer;
	void *prev;
	unsigned char *buffer;
	const unsigned char *p;
	size_t l, bufsize, buflen;
	ssize_t r;
	
	prev = current;
	importer = NULL;
	for(l = 0; l < cbcount; l++)
	{
		if(callbacks[l].type != TCB_BULK)
		{
			continue;
		}
		if(!strcmp(callbacks[l].m.bulk.type, mimetype))
		{
			importer = &(callbacks[l]);
			break;
		}
	}
	if(!importer)
	{
		twine_logf(LOG_ERR, "no bulk importer registered for '%s'\n", mimetype);
		return -1;
	}
	buffer = NULL;
	bufsize = 0;
	buflen = 0;
	current = importer->module;
	while(!feof(file))
	{
		if(bufsize - buflen < 1024)
		{
			p = (unsigned char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				twine_logf(LOG_CRIT, "failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				free(buffer);
				current = prev;
				return -1;
			}
			buffer = (unsigned char *) p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, file);
		if(r < 0)
		{
			twine_logf(LOG_CRIT, "I/O error during bulk import: %s\n", strerror(errno));
			free(buffer);
			current = prev;
			return -1;
		}
		buflen += r;
		buffer[buflen] = 0;
		if(!buflen)
		{
			/* Nothing new was read */
			continue;
		}
		p = importer->m.bulk.fn(importer->m.bulk.type, buffer, buflen, importer->data);
		if(!p)
		{
			twine_logf(LOG_ERR, "bulk importer failed\n");
			free(buffer);
			current = prev;
			return -1;
		}
		if(p == buffer)
		{
			continue;
		}
		if(p < buffer || p > buffer + buflen)
		{
			twine_logf(LOG_ERR, "bulk importer returned a buffer pointer out of bounds\n");
			free(buffer);
			current = prev;
			return -1;
		}
		l = buflen - (p - buffer);
		memmove(buffer, p, l);
		buflen = l;
	}
	if(buflen)
	{
		p = importer->m.bulk.fn(importer->m.bulk.type, buffer, buflen, importer->data);
		if(!p)
		{
			twine_logf(LOG_ERR, "bulk importer failed\n");
			free(buffer);
			current = prev;
			return -1;			
		}
	}
	current = prev;
	free(buffer);
	return 0;
}

/* Public: Ask a plug-in to update its caches about an identifier */
int
twine_update(const char *name, const char *identifier)
{
	struct twine_callback_struct *plugin;
	void *prev;
	size_t l;
	int r;
	
	prev = current;
	plugin = NULL;
	for(l = 0; l < cbcount; l++)
	{
		if(callbacks[l].type != TCB_UPDATE)
		{
			continue;
		}
		if(!strcmp(callbacks[l].m.update.name, name))
		{
			plugin = &(callbacks[l]);
			break;
		}
	}
	if(!plugin)
	{
		twine_logf(LOG_ERR, "no update handler '%s' has been registered\n", name);
		return -1;
	}
	current = plugin->module;
	r = plugin->m.update.fn(plugin->m.update.name, identifier, plugin->data);
	current = prev;
	if(r)
	{
		twine_logf(LOG_ERR, "handler '%s' failed to update\n", name);
		return -1;
	}
	return 0;
}

/* Internal: invoke post-processing handlers before a graph is replaced */
int
twine_preproc_process_(twine_graph *graph)
{
	void *prev;
	size_t c;
	int r;

	twine_logf(LOG_DEBUG, "invoking pre-processors for <%s>\n", graph->uri);
	prev = current;
	r = 0;
	for(c = 0; c < cbcount; c++)
	{
		if(callbacks[c].type == TCB_GRAPH &&
		   !strncmp(callbacks[c].m.graph.name, "pre:", 4))
		{
			current = callbacks[c].module;
			if(callbacks[c].m.graph.fn(graph, callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", callbacks[c].m.graph.name);
				r = -1;
				break;
			}
		}
	}
	current = prev;
	return r;
}

/* Internal: invoke post-processing handlers after a graph has been replaced */
int
twine_postproc_process_(twine_graph *graph)
{
	size_t c;
	void *prev;
	int r;

	twine_logf(LOG_DEBUG, "invoking post-processors for <%s>\n", graph->uri);
	prev = current;
	r = 0;
	for(c = 0; c < cbcount; c++)
	{
		if(callbacks[c].type == TCB_GRAPH &&
		   !strncmp(callbacks[c].m.graph.name, "post:", 5))
		{
			current = callbacks[c].module;
			if(callbacks[c].m.graph.fn(graph, callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", callbacks[c].m.graph.name);
				r = -1;
				break;
			}
		}
	}
	current = prev;
	return r;
}

/* Internal: invoke a graph processing handler */
int
twine_graph_process_(const char *name, twine_graph *graph)
{
	void *prev;
	size_t c;
	int r;

	twine_logf(LOG_DEBUG, "invoking graph processor '%s' for <%s>\n", name, graph->uri);
	prev = current;
	r = 0;
	for(c = 0; c < cbcount; c++)
	{
		if(callbacks[c].type == TCB_GRAPH &&
		   !strcmp(callbacks[c].m.graph.name, name))
		{
			current = callbacks[c].module;
			if(callbacks[c].m.graph.fn(graph, callbacks[c].data))
			{
				twine_logf(LOG_ERR, "graph processor '%s' failed\n", callbacks[c].m.graph.name);
				r = -1;
			}
			break;
		}
	}
	current = prev;
	return r;
}

/* Private: add a new callback */
static struct twine_callback_struct *
twine_plugin_callback_add_(void *data)
{
	struct twine_callback_struct *p;

	if(!current && !internal)
	{
		twine_logf(LOG_ERR, "attempt to register a new callback outside of a module\n");
		return NULL;
	}
	if(cbcount >= cbsize)
	{
		p = (struct twine_callback_struct *) realloc(callbacks, sizeof(struct twine_callback_struct) * (cbsize + CALLBACK_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, "failed to allocate memory to register callback\n");
			return NULL;
		}
		callbacks = p;
		cbsize += CALLBACK_BLOCKSIZE;
	}
	p = &(callbacks[cbcount]);
	memset(p, 0, sizeof(struct twine_callback_struct));
	p->context = twine_;
	p->module = current;
	p->data = data;
	cbcount++;
	return p;
}

/* Private: Load all configured plug-ins into a context */
int
twine_plugin_init_(TWINE *context)
{
	int r;

	r = twine_config_get_all("plugins", "module", twine_plugin_config_cb_, context);
	if(r < 0)
	{
		return -1;
	}
	else if(r)
	{
		if(context->appname && strcmp(context->appname, DEFAULT_CONFIG_SECTION_NAME))
		{
			twine_logf(LOG_NOTICE, "The [plugins] configuration section has been deprecated; you should use the common [%s] section or application-specific [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME, context->appname);
		}
		else
		{
			twine_logf(LOG_NOTICE, "The [plugins] configuration section has been deprecated; you should use the common [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME);
		}
		return 0;
	}
	return twine_config_get_all("*", "module", twine_plugin_config_cb_, context);
}

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
