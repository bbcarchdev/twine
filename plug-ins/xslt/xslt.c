/* Twine: XSLT processor
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
#include <ctype.h>
#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include "libtwine.h"

#define TWINE_PLUGIN_NAME               "xslt"
#define XSLT_MIME_LEN                   63

struct xslt_mime_struct
{
	struct xslt_mime_struct *next;
	TWINE *context;
	char mimetype[XSLT_MIME_LEN+1];
	char *desc;
	char *path;
	char *xpath;
	xmlDocPtr doc;
	xsltStylesheetPtr xslt;
};

static struct xslt_mime_struct *first, *last;

static int xslt_process(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, const char *restrict subject, void *data);
static int xslt_process_buf(TWINE *restrict context, const char *restrict buf, size_t buflen, xsltStylesheetPtr stylesheet, const char *restrict xpath);
static int xslt_config_cb(const char *key, const char *value, void *data);
static int xslt_mime_add(TWINE *restrict context, const char *restrict mimetype);
static struct xslt_mime_struct *xslt_mime_find(TWINE *restrict context, const char *restrict mimetype);

/* Twine plug-in entry-point */
int
twine_entry(TWINE *context, TWINEENTRYTYPE type, void *handle)
{
	struct xslt_mime_struct *p;
	size_t c;

	(void) handle;

	switch(type)
	{
	case TWINE_ATTACHED:
		twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME " plug-in: initialising\n");
		twine_config_get_all(NULL, NULL, xslt_config_cb, context);
		c = 0;
		for(p = first; p; p = p->next)
		{
			if(!p->path)
			{
				twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": MIME type '%s' cannot be registered because no path to a stylesheet was provided\n", p->mimetype);
				continue;
			}
			if(!p->xpath)
			{
				twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": MIME type '%s' cannot be registered because no XPath expression for graph URIs was provided\n", p->mimetype);
				continue;
			}
			p->doc = xmlParseFile(p->path);
			if(!p->doc)
			{
				twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to parse '%s' as well-formed XML\n", p->path);
				continue;
			}
			p->xslt = xsltParseStylesheetDoc(p->doc);
			if(!p->xslt)
			{
				twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": failed to process '%s' as an XSLT stylesheet\n", p->path);
				continue;
			}
			c++;
			twine_plugin_add_input(context, p->mimetype, p->desc, xslt_process, NULL);
		}
		if(!c)
		{
			twine_logf(LOG_WARNING, TWINE_PLUGIN_NAME ": no MIME types registered\n");
			return 0;
		}
		break;
	case TWINE_DETACHED:
		break;
	}
	return 0;
}

/* Find configuration entries in sections whose names begin with
 * 'xslt: <MIME TYPE>'
 */
static int
xslt_config_cb(const char *key, const char *value, void *data)
{
	char mimebuf[XSLT_MIME_LEN + 1], *t;
	struct xslt_mime_struct *mime;
	TWINE *context;

	context = (TWINE *) data;

	if(strncmp(key, "xslt:", 5))
	{
		return 0;
	}
	key += 5;
	while(isspace(*key))
	{
		key++;
	}
	if(!value)
	{
		if(xslt_mime_add(context, key))
		{
			return -1;
		}
		return 0;
	}
	t = strchr(key, ':');
	if(!t || (t - key) >= XSLT_MIME_LEN)
	{
		/* This shouldn't ever happen */
		return 0;
	}
	strncpy(mimebuf, key, t - key);
	mimebuf[t - key] = 0;
	t++;
	mime = xslt_mime_find(context, mimebuf);
	if(!mime)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": unable to locate internal MIME type structure for '%s'\n", mimebuf);
		return -1;
	}
	if(!strcmp(t, "desc"))
	{
		if(!mime->desc)
		{
			mime->desc = strdup(value);
			if(!mime->desc)
			{
				twine_logf(LOG_CRIT, TWINE_PLUGIN_NAME ": failed to allocate string for MIME type description for '%s'\n", mimebuf);
				return -1;
			}
		}
		else
		{
			twine_logf(LOG_WARNING, TWINE_PLUGIN_NAME ": description of '%s' specified more than once; only the first will take effect\n", mimebuf);
		}
	}
	else if(!strcmp(t, "xslt"))
	{
		if(!mime->path)
		{
			mime->path = strdup(value);
			if(!mime->path)
			{
				twine_logf(LOG_CRIT, TWINE_PLUGIN_NAME ": failed to allocate string for XSLT stylesheet path for '%s'\n", mimebuf);
				return -1;
			}
		}
		else
		{
			twine_logf(LOG_WARNING, TWINE_PLUGIN_NAME ": XSLT stylesheet path for '%s' specified more than once; only the first will take effect\n", mimebuf);
		}
	}
	else if(!strcmp(t, "graph-uri"))
	{
		if(!mime->xpath)
		{
			mime->xpath = strdup(value);
			if(!mime->xpath)
			{
				twine_logf(LOG_CRIT, TWINE_PLUGIN_NAME ": failed to allocate string for graph URI XPath expression for '%s'\n", mimebuf);
				return -1;
			}
		}
		else
		{
			twine_logf(LOG_WARNING, TWINE_PLUGIN_NAME ": graph URI XPath expression for '%s' specified more than once; only the first will take effect\n", mimebuf);
		}
	}
	else
	{
		twine_logf(LOG_WARNING, TWINE_PLUGIN_NAME ": unrecognised key '%s' while processing configuration of '%s'\n", t, mimebuf);
	}
	return 0;
}

static int
xslt_mime_add(TWINE *context, const char *mimetype)
{
	struct xslt_mime_struct *p;
	
	if(strlen(mimetype) > XSLT_MIME_LEN)
	{
		twine_logf(LOG_ERR, TWINE_PLUGIN_NAME ": cannot add MIME type '%s' because it is too long\n", mimetype);
		return -1;
	}
	p = (struct xslt_mime_struct *) calloc(1, sizeof(struct xslt_mime_struct));
	if(!p)
	{
		twine_logf(LOG_CRIT, TWINE_PLUGIN_NAME ": failed to allocate %u bytes for MIME type information\n", (unsigned) sizeof(struct xslt_mime_struct));
		return -1;
	}
	p->context = context;
	strcpy(p->mimetype, mimetype);
	if(last)
	{
		last->next = p;
		last = p;
	}
	else
	{
		first = last = p;
	}
	twine_logf(LOG_DEBUG, TWINE_PLUGIN_NAME ": added MIME type '%s'\n", mimetype);
	return 0;
}

static struct xslt_mime_struct *
xslt_mime_find(TWINE *context, const char *mimetype)
{
	struct xslt_mime_struct *p;

	for(p = first; p; p = p->next)
	{
		if(p->context == context &&
		   !strcasecmp(p->mimetype, mimetype))
		{
			return p;
		}
	}
	return NULL;
}


/* Process a block of XML using the confgiured stylesheet and XPath expression
 */
static int
xslt_process(TWINE *restrict context, const char *restrict mime, const unsigned char *restrict buf, size_t buflen, const char *restrict subject, void *data)
{
	struct xslt_mime_struct *p;

	(void) data;
	(void) subject;

	p = xslt_mime_find(context, mime);
	if(!p)
	{
		twine_logf(LOG_CRIT, "unable to locate MIME type information for '%s'\n", mime);
		return -1;
	}
	return xslt_process_buf(context, (const char *) buf, buflen, p->xslt, p->xpath);
}	

/* Process a buffer using an XSLT stylesheet, and evaluate an XPath expression
 * in order to determine the URI of the graph which the resulting RDF/XML will
 * replace.
 */
static int
xslt_process_buf(TWINE *context, const char *restrict buf, size_t buflen, xsltStylesheetPtr stylesheet, const char *xpath)
{
	xmlDocPtr xmldoc, res;
	xmlChar *xmlbuf;
	xmlXPathContextPtr xpctx;
	xmlXPathObjectPtr xpobj;
	int xmlbuflen;
	librdf_model *model;
	librdf_stream *stream;
	char *p;

	/* Parse the incoming buffer as an XML document */
	xmldoc = xmlParseMemory(buf, buflen);
	if(!xmldoc)
	{
		twine_logf(LOG_ERR, "failed to parse XML document\n");
		return -1;
	}
	/* Apply the XSLT stylesheet to the parsed XML document */
	res = xsltApplyStylesheet(stylesheet, xmldoc, NULL);
	if(!res)
	{
		twine_logf(LOG_ERR, "failed to apply stylesheet to XML document\n");
		xmlFreeDoc(xmldoc);
		return -1;
	}
	/* Obtain the results of applying the stylesheet as a string buffer */
	xmlbuf = NULL;
	xmlbuflen = 0;
	if(xsltSaveResultToString(&xmlbuf, &xmlbuflen, res, stylesheet))
	{
		twine_logf(LOG_ERR, "failed to store processed XML\n");
		xmlFreeDoc(res);
		xmlFreeDoc(xmldoc);
		return -1;
	}
	xmlFreeDoc(res);
	/* Create an RDF model to store the graph in */
	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_ERR, "failed to create a new RDF model\n");
		free(xmlbuf);
		xmlFreeDoc(xmldoc);
		return -1;
	}
	/* Parse the RDF/XML result of the stylesheet */
	if(twine_rdf_model_parse(model, "application/rdf+xml", (const char *) xmlbuf, (size_t) xmlbuflen))
	{
		twine_logf(LOG_ERR, "failed to parse transformed RDF/XML into RDF model\n");
		free(xmlbuf);
		librdf_free_model(model);
		xmlFreeDoc(xmldoc);
		return -1;
	}	
	free(xmlbuf);
	/* Find the subject in the form <http://example.com/things/ID#id> */
	xpctx = xmlXPathNewContext(xmldoc);
	if(!xpctx)
	{
		twine_logf(LOG_CRIT, "failed to create new XPath context from XML document\n");
		librdf_free_model(model);
		xmlFreeDoc(xmldoc);
		return -1;
	}
	xpobj = xmlXPathEvalExpression((const xmlChar *) xpath, xpctx);
	if(!xpobj)
	{
		twine_logf(LOG_ERR, "failed to evaluate Graph URI XPath expression: %s\n", xpath);
		librdf_free_model(model);
		xmlXPathFreeContext(xpctx);
		xmlFreeDoc(xmldoc);
		return -1;
	}
	if(xpobj->type != XPATH_STRING)
	{
		twine_logf(LOG_ERR, "Graph URI XPath expression did not result in a string node\n");
		librdf_free_model(model);
		xmlXPathFreeContext(xpctx);
		xmlFreeDoc(xmldoc);
		return -1;
	}
	twine_logf(LOG_DEBUG, "Graph URI XPath result: <%s>\n", xpobj->stringval);
	p = strdup((const char *) xpobj->stringval);
	if(!p)
	{
		twine_logf(LOG_CRIT, "failed to duplicate graph URI XPath result string\n");
		librdf_free_model(model);
		xmlXPathFreeContext(xpctx);
		xmlFreeDoc(xmldoc);
		return -1;
	}		
	xmlXPathFreeContext(xpctx);
	xmlFreeDoc(xmldoc);
	/* Replace the graph */
	stream = librdf_model_as_stream(model);	
	twine_workflow_process_stream(context, p, stream);
	/* Clean up */
	free(p);
	librdf_free_stream(stream);
	librdf_free_model(model);
	return 0;
}
