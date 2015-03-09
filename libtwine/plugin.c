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
static int preprocessing, postprocessing;

static size_t mimecount, bulkcount, precount, postcount;

static struct twine_callback_struct *callbacks;
static size_t cbcount, cbsize;

static struct twine_callback_struct *twine_plugin_callback_add_(void *data);

/* Internal: load a plug-in and invoke its initialiser callback */

int
twine_plugin_load_(const char *pathname)
{
	void *handle;
	twine_plugin_init_fn fn;
	char *fnbuf;
	size_t len;

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
			return -1;
		}
		strcpy(fnbuf, PLUGINDIR);
		strcat(fnbuf, pathname);
		pathname = fnbuf;
	}
	handle = dlopen(pathname, RTLD_NOW);
	if(!handle)
	{
		twine_logf(LOG_ERR, "failed to load %s: %s\n", pathname, dlerror());
		free(fnbuf);
		return -1;
	}
	fn = (twine_plugin_init_fn) dlsym(handle, "twine_plugin_init");
	if(!fn)
	{
		twine_logf(LOG_ERR, "%s is not a Twine plug-in\n", pathname);
		dlclose(handle);
		free(fnbuf);
		errno = EINVAL;
		return -1;
	}
	twine_logf(LOG_DEBUG, "invoking plug-in initialisation function for %s\n", pathname);
	current = handle;
	if(fn())
	{
		twine_logf(LOG_DEBUG, "initialisation of plug-in %s failed\n", pathname);
		current = NULL;
		twine_plugin_unregister_all_(handle);		
		dlclose(handle);
		free(fnbuf);
		return -1;
	}
	twine_logf(LOG_INFO, "loaded plug-in %s\n", pathname);
	free(fnbuf);
	current = NULL;
	return 0;
}

/* Internal: unload all plug-ins */
int
twine_plugin_unload_all_(void)
{
	twine_plugin_cleanup_fn fn;
	void *handle;

	while(cbcount)
	{
		handle = callbacks[0].module;
		twine_plugin_unregister_all_(handle);
		if(cbcount && callbacks[0].module == handle)
		{
			twine_logf(LOG_ERR, "failed to unregister callbacks for handle 0x%08x; aborting clean-up\n", (unsigned long) handle);
			return -1;
		}
		fn = (twine_plugin_cleanup_fn) dlsym(handle, "twine_plugin_done");
		if(fn)
		{
			current = handle;
			fn();
			current = NULL;
		}		
	}
	free(callbacks);
	callbacks = NULL;
	cbcount = 0;
	cbsize = 0;
	twine_logf(LOG_INFO, "all plug-ins unregistered\n");
	return 0;
}


/* Internal: add a new callback */
static struct twine_callback_struct *
twine_plugin_callback_add_(void *data)
{
	struct twine_callback_struct *p;

	if(!current)
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
	p->module = current;
	p->data = data;
	cbcount++;
	return p;
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
	mimecount++;
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
	bulkcount++;
	twine_logf(LOG_INFO, "registered bulk processor for MIME type: '%s' (%s)\n", mimetype, description);
	return 0;
}

/* Public: Register a post-processing module */
int
twine_postproc_register(const char *name, twine_postproc_fn fn, void *data)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(data);
	if(!p)
	{
		return -1;
	}
	p->m.postproc.name = strdup(name);
	if(!p->m.postproc.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register post-processor\n");
		return -1;
	}
	p->m.postproc.fn = fn;
	p->type = TCB_POSTPROC;
	postcount++;
	twine_logf(LOG_INFO, "registered post-processor module: '%s'\n", name);
	return 0;
}

/* Public: Register a pre-processing module */
int
twine_preproc_register(const char *name, twine_postproc_fn fn, void *data)
{
	struct twine_callback_struct *p;

	p = twine_plugin_callback_add_(data);
	if(!p)
	{
		return -1;
	}
	p->m.preproc.name = strdup(name);
	if(!p->m.preproc.name)
	{
		twine_logf(LOG_CRIT, "failed to allocate memory to register pre-processor\n");
		return -1;
	}
	p->m.preproc.fn = fn;
	p->type = TCB_PREPROC;
	precount++;
	twine_logf(LOG_INFO, "registered pre-processor module: '%s'\n", name);
	return 0;
}

/* Internal: un-register all plugins attached to a module */
int
twine_plugin_unregister_all_(void *handle)
{
	size_t l;

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
		case TCB_PREPROC:
			free(callbacks[l].m.preproc.name);
			break;
		case TCB_POSTPROC:
			free(callbacks[l].m.postproc.name);
			break;
		}
		if(l + 1 < cbcount)
		{
			memmove(&(callbacks[l]), &(callbacks[l + 1]), sizeof(struct twine_callback_struct) * (cbcount - l - 1));
		}
		cbcount--;
	}
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

/* Internal: return nonzero if any postprocessors have been registered */
int
twine_postproc_registered_(void)
{
	return (postcount ? 1 : 0);
}

/* Internal: return nonzero if any preprocessors have been registered */
int
twine_preproc_registered_(void)
{
	return (precount ? 1 : 0);
}

/* Internal: invoke post-processing handlers before a graph is replaced */
int
twine_preproc_process_(twine_graph *graph)
{
	void *prev;
	size_t c;
	int r;

	if(!precount)
	{
		return 0;
	}
	if(preprocessing || postprocessing)
	{
		return 0;
	}
	twine_logf(LOG_DEBUG, "invoking pre-processors for <%s>\n", graph->uri);
	if(!graph->store)
	{
		graph->store = twine_rdf_model_clone(graph->pristine);
		if(!graph->store)
		{
			twine_logf(LOG_ERR, "failed to duplicate model for pre-processors\n");
			return -1;
		}
	}
	preprocessing = 1;
	prev = current;
	r = 0;
	for(c = 0; c < cbcount; c++)
	{
		if(callbacks[c].type == TCB_PREPROC)
		{
			current = callbacks[c].module;
			if(callbacks[c].m.preproc.fn(graph, callbacks[c].data))
			{
				twine_logf(LOG_ERR, "pre-processor '%s' failed\n", callbacks[c].m.preproc.name);
				r = -1;
				break;
			}
		}
	}
	current = prev;
	postprocessing = 0;
	return r;
}

/* Internal: invoke post-processing handlers after a graph has been replaced */
int
twine_postproc_process_(twine_graph *graph)
{
	size_t c;
	void *prev;
	int r;

	if(!postcount)
	{
		return 0;
	}
	if(postprocessing || postprocessing)
	{
		return 0;
	}
	twine_logf(LOG_DEBUG, "invoking post-processors for <%s>\n", graph->uri);
	postprocessing = 1;
	prev = current;
	r = 0;
	for(c = 0; c < cbcount; c++)
	{
		if(callbacks[c].type == TCB_POSTPROC)
		{
			current = callbacks[c].module;
			if(callbacks[c].m.postproc.fn(graph, callbacks[c].data))
			{
				twine_logf(LOG_ERR, "pre-processor '%s' failed\n", callbacks[c].m.preproc.name);
				r = -1;
				break;
			}
		}
	}
	current = prev;
	postprocessing = 0;
	return r;
}
