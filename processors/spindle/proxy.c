/* Spindle: Co-reference aggregation engine
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

#include "p_spindle.h"

/* Generate a new local URI for an external URI */
char *
spindle_proxy_generate(const char *uri)
{
	uuid_t uu;
	char uubuf[32];
	char *p, *t;
	size_t c;

	(void) uri;

	uuid_generate(uu);
	uuid_unparse_lower(uu, uubuf);
	
	p = (char *) calloc(1, strlen(spindle_root) + 48);
	if(!p)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for local URI\n");
		return NULL;
	}
	strcpy(p, spindle_root);
	t = strchr(p, 0);
	for(c = 0; uubuf[c]; c++)
	{
		if(isalnum(uubuf[c]))
		{
			*t = uubuf[c];
			t++;
		}
	}
	*t = '#';
	t++;
	*t = 'i';
	t++;
	*t = 'd';
	t++;
	*t = 0;
	return p;
}

/* Look up the local URI for an external URI in the store */
char *
spindle_proxy_locate(const char *uri)
{
	SPARQLRES *res;
	SPARQLROW *row;
	char *qbuf, *localname;
	size_t l;
	librdf_node *node;
	librdf_uri *ruri;

	/* TODO: if uri is within our namespace and is valid, return it as-is */
	errno = 0;	
	localname = NULL;
	l = strlen(uri) + strlen(spindle_root) + 127;
	qbuf = (char *) calloc(1, l + 1);
	if(!l)
	{
		return NULL;
	}   
	snprintf(qbuf, l, "SELECT DISTINCT ?o FROM <%s> WHERE { <%s> <http://www.w3.org/2002/07/owl#sameAs> ?o . }", spindle_root, uri);
	res = sparql_query(spindle_sparql, qbuf, strlen(qbuf));
	if(!res)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query for existence of <%s> in <%s>\n", uri, spindle_root);
		free(qbuf);
		return NULL;
	}
	free(qbuf);
	row = sparqlres_next(res);
	if(row)
	{
		node = sparqlrow_binding(row, 0);
		if(node && librdf_node_is_resource(node))
		{
			ruri = librdf_node_get_uri(node);
			localname = strdup((const char *) librdf_uri_as_string(ruri));
			if(!localname)
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to duplicate URI string\n");				
			}
		}
	}
	sparqlres_destroy(res);
	return localname;
}

/* Assert that two URIs are equivalent */
int
spindle_proxy_create(const char *uri1, const char *uri2)
{
	char *u1, *u2, *uu;

	u1 = spindle_proxy_locate(uri1);
	u2 = spindle_proxy_locate(uri2);
	if(u1 && u2 && !strcmp(u1, u2))
	{
		/* The coreference already exists */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": <%s> <=> <%s> already exists\n", uri1, uri2);		
		return 0;
	}
	uu = (u1 ? u1 : (u2 ? u2 : NULL));
	if(!uu)
	{
		uu = spindle_proxy_generate(uri1);
		if(!uu)
		{
			return -1;
		}
	}
	if(!u1)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": adding <%s> owl:sameAs <%s>\n", uri1, uu);
		spindle_proxy_relate(uri1, uu);
		/* <uri1> owl:sameAs <uu> */
	}
	if(!u2)
	{
		spindle_proxy_relate(uri2, uu);
		/* <uri2> owl:sameAs <uu> */
	}
	else if(strcmp(u2, uu))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": relocating references from <%s> to <%s>\n", u2, uu);
		spindle_proxy_migrate(u2, uu, NULL);
	}
	free(u1);
	free(u2);
	if(uu != u1 && uu != u2)
	{
		free(u2);
	}
	return 0;
}

/* Move a set of references from one proxy to another */
int
spindle_proxy_migrate(const char *from, const char *to, char **refs)
{
	size_t c;

	if(!refs)
	{
		refs = spindle_proxy_refs(from);
	}
	/* dels = 'DELETE DATA { GRAPH <root> { ' */
	for(c = 0; refs[c]; c++)
	{
		/* dels += '<ref> owl:sameAs <from>' */
		/* adds += '<ref> owl:sameAs <to>' */
	}
	return 0;
}

/* Obtain all of the outbound references from a proxy */
char *
spindle_proxy_refs(const char *uri)
{
	/* SELECT DISTINCT ?s FROM <root> WHERE { ?s owl:sameAs <uri> . } */
	return NULL;
}

/* Store a relationship between a proxy and a processed entity */
int
spindle_proxy_relate(const char *remote, const char *local)
{
	size_t l;
	char *qbuf;
	int r;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": adding <%s> (remote) owl:sameAs <%s> (local)\n", remote, local);
	l = strlen(spindle_root) + strlen(remote) + strlen(local) + 127;
	qbuf = (char *) calloc(1, l + 1);
	if(!qbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate SPARQL query buffer\n");
		return -1;
	}
	snprintf(qbuf, l, "PREFIX owl: <http://www.w3.org/2002/07/owl#>\nINSERT DATA {\nGRAPH <%s> {\n<%s> owl:sameAs <%s> . } }", spindle_root, remote, local);
	twine_logf(LOG_DEBUG, "%s\n", qbuf);
	r = sparql_update(spindle_sparql, qbuf, strlen(qbuf));
	free(qbuf);
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": SPARQL INSERT DATA failed\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": INSERT succeeded\n");
	return 0;
}
