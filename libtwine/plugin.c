/* Twine: Plug-in handling
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

#define MIMETYPE_BLOCKSIZE              4
#define POSTPROC_BLOCKSIZE              4
#define PLUGINDIR                       LIBDIR "/twine/"

static void *current;
static int postprocessing;

static struct twine_mime_struct *mimetypes;
static size_t mimecount, mimesize;

static struct twine_bulk_struct *bulktypes;
static size_t bulkcount, bulksize;

static struct twine_postproc_struct *postprocs;
static size_t ppcount, ppsize;

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

/* Public: register a MIME type */
int
twine_plugin_register(const char *mimetype, const char *description, twine_processor_fn fn, void *data)
{
	struct twine_mime_struct *p;

	if(!current)
	{
		twine_logf(LOG_ERR, "attempt to register MIME type '%s' outside of plug-in initialisation\n", mimetype);
		return -1;
	}
	if(mimecount >= mimesize)
	{
		p = (struct twine_mime_struct *) realloc(mimetypes, sizeof(struct twine_mime_struct) * (mimesize + MIMETYPE_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
			return -1;
		}
		mimetypes = p;
		mimesize += MIMETYPE_BLOCKSIZE;
	}
	p = &(mimetypes[mimecount]);
	p->module = current;
	p->mimetype = strdup(mimetype);
	p->desc = strdup(description);
	if(!p->mimetype || !p->desc)
	{
		free(p->mimetype);
		free(p->desc);
		twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
		return -1;
	}
	twine_logf(LOG_INFO, "registered MIME type: '%s' (%s)\n", mimetype, description);
	p->processor = fn;
	p->data = data;
	mimecount++;
	return 0;
}
/* Public: register a bulk processor for a MIME type */
int
twine_bulk_register(const char *mimetype, const char *description, twine_bulk_fn fn, void *data)
{
	struct twine_bulk_struct *p;

	if(!current)
	{
		twine_logf(LOG_ERR, "attempt to register MIME type '%s' outside of plug-in initialisation\n", mimetype);
		return -1;
	}
	if(bulkcount >= bulksize)
	{
		p = (struct twine_bulk_struct *) realloc(bulktypes, sizeof(struct twine_bulk_struct) * (bulksize + MIMETYPE_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
			return -1;
		}
		bulktypes = p;
		bulksize += MIMETYPE_BLOCKSIZE;
	}
	p = &(bulktypes[bulkcount]);
	p->module = current;
	p->mimetype = strdup(mimetype);
	p->desc = strdup(description);
	if(!p->mimetype || !p->desc)
	{
		free(p->mimetype);
		free(p->desc);
		twine_logf(LOG_CRIT, "failed to allocate memory to register MIME type '%s'\n", mimetype);
		return -1;
	}
	twine_logf(LOG_INFO, "registered MIME type: '%s' (%s)\n", mimetype, description);
	p->processor = fn;
	p->data = data;
	bulkcount++;
	return 0;
}

/* Register a post-processing module */
int
twine_postproc_register(const char *name, twine_postproc_fn fn, void *data)
{
	struct twine_postproc_struct *p;

	if(ppcount >= ppsize)
	{
		p = (struct twine_postproc_struct *) realloc(postprocs, sizeof(struct twine_postproc_struct) * (ppsize + POSTPROC_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, "failed to allocate memory to register post-processing module '%s'\n", name);
			return -1;
		}
		postprocs = p;
		ppsize += POSTPROC_BLOCKSIZE;
	}
	p = &(postprocs[ppcount]);
	p->module = current;
	p->fn = fn;
	p->data = data;
	p->name = strdup(name);
	if(!p->name)
	{
		return -1;
	}
	ppcount++;
	twine_logf(LOG_INFO, "registered post-processor module: '%s'\n", name);
	return 0;
}

/* Internal: un-register all plugins attached to a module */
int
twine_plugin_unregister_all_(void *handle)
{
	size_t l;

	l = 0;
	while(l < mimecount)
	{
		if(mimetypes[l].module != handle)
		{
			l++;
			continue;
		}
		twine_logf(LOG_INFO, "deregistering MIME type '%s'\n", mimetypes[l].mimetype);
		free(mimetypes[l].mimetype);
		free(mimetypes[l].desc);
		if(l + 1 < mimecount)
		{
			memmove(&(mimetypes[l]), &(mimetypes[l + 1]), sizeof(struct twine_mime_struct) * (mimecount - l - 1));
		}
		mimecount--;
	}
	l = 0;
	while(l < ppcount)
	{
		if(postprocs[l].module != handle)
		{
			l++;
			continue;
		}
		twine_logf(LOG_INFO, "deregistering post-processor '%s'\n", postprocs[l].name);
		free(postprocs[l].name);
		if(l + 1 < ppcount)
		{
			memmove(&(postprocs[l]), &(postprocs[l + 1]), sizeof(struct twine_postproc_struct) * (ppcount - l - 1));
		}
		ppcount--;
	}
	return 0;
}

/* Internal: forward a message to a plug-in for processing */
int
twine_plugin_process_(const char *mimetype, const char *message, size_t msglen)
{
	size_t l;
	void *prev;
	int r;

	prev = current;
	for(l = 0; l < mimecount; l++)
	{
		if(!strcmp(mimetypes[l].mimetype, mimetype))
		{
			current = mimetypes[l].module;
			r = mimetypes[l].processor(mimetype, message, msglen, mimetypes[l].data);
			current = prev;
			return r;
		}
	}
	twine_logf(LOG_ERR, "no available processor for messages of type '%s'\n", mimetype);
	return -1;
}

/* Check whether a MIME type is supported by any processor plugin */
int
twine_plugin_supported(const char *mimetype)
{
	size_t l;
	for(l = 0; l < mimecount; l++)
	{
		if(!strcmp(mimetypes[l].mimetype, mimetype))
		{
			return 1;
		}
	}
	return 0;
}

/* Check whether a MIME type is supported by any bulk processor plugin */
int
twine_bulk_supported(const char *mimetype)
{
	size_t l;

	for(l = 0; l < bulkcount; l++)
	{
		if(!strcmp(bulktypes[l].mimetype, mimetype))
		{
			return 1;
		}
	}
	return 0;
}

/* Perform a bulk import from a file */
int twine_bulk_import(const char *mimetype, FILE *file)
{
	struct twine_bulk_struct *importer;
	void *prev;
	char *buffer;
	const char *p;
	size_t l, bufsize, buflen;
	ssize_t r;
	
	prev = current;
	importer = NULL;
	for(l = 0; l < bulkcount; l++)
	{
		if(!strcmp(bulktypes[l].mimetype, mimetype))
		{
			importer = &(bulktypes[l]);
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
			p = (char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				twine_logf(LOG_CRIT, "failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				free(buffer);
				current = prev;
				return -1;
			}
			buffer = (char *) p;
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
		p = importer->processor(importer->mimetype, buffer, buflen, importer->data);
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
		p = importer->processor(importer->mimetype, buffer, buflen, importer->data);
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
	return (ppcount ? 1 : 0);
}

/* Internal: invoke post-processing handlers after a graph has been replaced */
int
twine_postproc_process_(librdf_model *newgraph, librdf_model *oldgraph, const char *graphuri)
{
	size_t c;
	void *prev;

	if(postprocessing)
	{
		return 0;
	}
	twine_logf(LOG_DEBUG, "invoking post-processors for <%s>\n", graphuri);
	postprocessing = 1;
	prev = current;
	for(c = 0; c < ppcount; c++)
	{
		current = postprocs[c].module;
		postprocs[c].fn(newgraph, oldgraph, graphuri, postprocs[c].data);
	}
	current = prev;
	postprocessing = 0;
	return 0;
}
