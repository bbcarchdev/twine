/* Twine: Example XSLT transform
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include "libtwine.h"

/* This processor accepts an XML document (with the MIME type
 * application/x-example+xml) and transforms it using a built-in
 * XSLT transform into RDF/XML. The RDF/XML is then parsed using
 * librdf and pushed into the quad-store.
 *
 * The XSLT generates instance URIs in the form:
 *
 *   http://example.com/things/<ID>#id
 *
 * From this, the graph URI is derived as:
 *
 *   http://example.com/things/<ID>
 *
 * (i.e., the fragment is stripped out)
 *
 * A more advanced processor could add additional triples into
 * the graph before submission if desirable.
 */

static const char *xsltbuf = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	"<xsl:stylesheet xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0' xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#' xmlns:dct='http://purl.org/dc/terms/'>"
	"<xsl:template match='/node()'>"
	"  <xsl:variable name='id'><xsl:copy-of select='/*/id' /></xsl:variable>"
	"  <rdf:RDF>"
	"    <rdf:Description>"
	"      <xsl:attribute name='rdf:about'>http://example.com/things/<xsl:value-of select='$id' />#id</xsl:attribute>"
	"      <dct:title><xsl:copy-of select='/*/title/node()' /></dct:title>"
	"      <dct:description><xsl:copy-of select='/*/description/node()' /></dct:description>"
	"    </rdf:Description>"
	"  </rdf:RDF>"
	"</xsl:template>"
	"</xsl:stylesheet>";

static const char *xpath = "concat('http://example.com/things/', string(/item/id))";

static xmlDocPtr xsltdoc;
static xsltStylesheetPtr xslt;
static int process_example_xml(const char *mime, const char *buf, size_t buflen);

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{
	twine_logf(LOG_DEBUG, "example-xml plug-in: initialising\n");
	twine_plugin_register("application/x-example+xml", "Example XSLT-transformable XML", process_example_xml);
	return 0;
}


static int
process_example_xml(const char *mime, const char *buf, size_t buflen)
{
	xmlDocPtr xmldoc, res;
	xmlChar *xmlbuf;
	xmlXPathContextPtr xpctx;
	xmlXPathObjectPtr xpobj;
	int xmlbuflen;
	librdf_model *model;
	librdf_stream *stream;
	char *p;

	(void) mime;

	/* If the XSLT document hasn't yet been parsed, do so now */
	if(!xsltdoc)
	{
		xmlSubstituteEntitiesDefault(1);
		xsltdoc = xmlParseMemory(xsltbuf, strlen(xsltbuf));
		if(!xsltdoc)
		{
			twine_logf(LOG_CRIT, "failed to parse XSLT into XML document\n");
			return -1;
		}
		xslt = xsltParseStylesheetDoc(xsltdoc);
		if(!xslt)
		{
			twine_logf(LOG_CRIT, "failed to parse stylesheet XML document into stylesheet\n");
			return -1;
		}
	}
	/* Parse the incoming buffer as an XML document */
	xmldoc = xmlParseMemory(buf, buflen);
	if(!xmldoc)
	{
		twine_logf(LOG_ERR, "failed to parse XML document\n");
		return -1;
	}
	/* Apply the XSLT stylesheet to the parsed XML document */
	res = xsltApplyStylesheet(xslt, xmldoc, NULL);
	if(!res)
	{
		twine_logf(LOG_ERR, "failed to apply stylesheet to XML document\n");
		xmlFreeDoc(xmldoc);
		return -1;
	}
	/* Obtain the results of applying the stylesheet as a string buffer */
	xmlbuf = NULL;
	xmlbuflen = 0;
	if(xsltSaveResultToString(&xmlbuf, &xmlbuflen, res, xslt))
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
	twine_sparql_put_stream(p, stream);
	/* Clean up */
	free(p);
	librdf_free_stream(stream);
	librdf_free_model(model);
	return 0;
}
