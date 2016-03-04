/* Twine: Processor for ingesting RDF from the GeoNames dump format
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libtwine.h"

#define TWINE_PLUGIN_NAME               "geonames"

static const unsigned char *bulk_geonames(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, void *restrict data);

static char *
strnchr(const char *src, int ch, size_t max)
{
	const char *t;

	for(t = src; (size_t) (t - src) < max; t++)
	{
		if(!*t)
		{
			break;
		}
		if(*t == ch)
		{
			return (char *) t;
		}
	}
	return NULL;
}

/* Twine plug-in entry-point */
int
twine_entry(TWINE *context, TWINEENTRYTYPE type, void *handle)
{
	(void) handle;

	switch(type)
	{
	case TWINE_ATTACHED:
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME " plug-in: initialising\n");
		twine_plugin_add_bulk(context, "text/x-geonames-dump", "Geonames dump", bulk_geonames, NULL);
		break;
	case TWINE_DETACHED:
		break;
	}
	return 0;
}

/* Process a Geonames RDF dump into quads and import it.
 *
 * A Geonames dump consists of sequences of two lines. The first line is the
 * primary topic, the second is the RDF/XML which describes it. The graph name
 * is the primary topic, with 'about.rdf' appended to it.
 */

static const unsigned char *
bulk_geonames(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, void *data)
{
	char *graph;
	const char *rdfxml, *topic;
	const unsigned char *t, *p;
	size_t remaining;

	(void) mime;
	(void) data;

	if(!buflen)
	{
		return 0;
	}
	t = (unsigned char *) buf;
	while((size_t) (t - buf) < buflen)
	{
		topic = (const char *) t;
		remaining = buflen - (t - buf);
		p = (const unsigned char *) strnchr(topic, '\n', remaining);
		if(!p)
		{
			return (const unsigned char *) topic;
		}
		graph = (char *) calloc(1, (const char *) p - topic + 16);
		if(!graph)
		{
			twine_logf(LOG_CRIT, "failed to allocate buffer for graph name\n");
			return NULL;
		}
		strncpy(graph, topic, (const char *) p - topic);
		strcpy(&(graph[(const char *) p - topic]), "about.rdf");
		rdfxml = (const char *) p + 1;
		remaining = buflen - (rdfxml - (const char *) buf);
		t = (const unsigned char *) strnchr(rdfxml, '\n', remaining);
		if(!t)
		{
			free(graph);
			return (const unsigned char *) topic;
		}
		if(twine_workflow_process_rdf(context, graph, (const unsigned char *) rdfxml, (const char *) t - rdfxml, "application/rdf+xml"))
		{
			twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to process graph <%s>\n", graph);
			free(graph);
			return NULL;
		}
		t++;
		free(graph);
	}
	return t;
}
