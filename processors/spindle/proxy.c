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
spindle_proxy_generate(SPINDLE *spindle, const char *uri)
{
	uuid_t uu;
	char uubuf[32];
	char *p, *t;
	size_t c;

	(void) uri;

	uuid_generate(uu);
	uuid_unparse_lower(uu, uubuf);
	
	p = (char *) calloc(1, strlen(spindle->root) + 48);
	if(!p)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for local URI\n");
		return NULL;
	}
	strcpy(p, spindle->root);
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
spindle_proxy_locate(SPINDLE *spindle, const char *uri)
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
	l = strlen(uri) + strlen(spindle->root) + 127;
	qbuf = (char *) calloc(1, l + 1);
	if(!l)
	{
		return NULL;
	}   
	snprintf(qbuf, l, "SELECT DISTINCT ?o FROM <%s> WHERE { <%s> <http://www.w3.org/2002/07/owl#sameAs> ?o . }", spindle->root, uri);
	res = sparql_query(spindle->sparql, qbuf, strlen(qbuf));
	if(!res)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query for existence of <%s> in <%s>\n", uri, spindle->root);
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
spindle_proxy_create(SPINDLE *spindle, const char *uri1, const char *uri2, struct spindle_strset_struct *changeset)
{
	char *u1, *u2, *uu;

	u1 = spindle_proxy_locate(spindle, uri1);
	if(uri2)
	{
		u2 = spindle_proxy_locate(spindle, uri2);
	}
	else
	{
		u2 = NULL;
	}
	if(u1 && u2 && !strcmp(u1, u2))
	{
		/* The coreference already exists */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": <%s> <=> <%s> already exists\n", uri1, uri2);
		if(changeset)
		{
			spindle_strset_add(changeset, u1);
		}
		return 0;
	}
	else if(!uri2 && u1)
	{
		/* The lone subject already exists */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": <%s> already exists\n", uri1);
		if(changeset)
		{
			spindle_strset_add(changeset, u1);
		}
		return 0;
	}
	/* If both entities already have local proxies, we just pick the first
	 * to use as the new unified proxy.
	 *
	 * If only one entity has a proxy, just use that and attach the new
	 * referencing triples to it.
	 *
	 * If neither does, generate a new local name and populate it.
	 */
	uu = (u1 ? u1 : (u2 ? u2 : NULL));
	if(!uu)
	{
		uu = spindle_proxy_generate(spindle, uri1);
		if(!uu)
		{
			return -1;
		}
	}
	/* If the first entity didn't previously have a local proxy, attach it */
	if(!u1)
	{
		spindle_proxy_relate(spindle, uri1, uu);
	}
	/* If the second entity didn't previously have a local proxy, attach it */
	if(!u2)
	{
		if(uri2)
		{
			spindle_proxy_relate(spindle, uri2, uu);
		}
	}
	else if(strcmp(u2, uu))
	{
		/* However, if it did have a local proxy and it was different to
		 * the one we've chosen, migrate its references over, leaving a single
		 * unified proxy.
		 */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": relocating references from <%s> to <%s>\n", u2, uu);
		spindle_proxy_migrate(spindle, u2, uu, NULL);
		if(changeset)
		{
			spindle_strset_add(changeset, u2);
		}
	}
	if(changeset)
	{
		spindle_strset_add(changeset, uu);
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
spindle_proxy_migrate(SPINDLE *spindle, const char *from, const char *to, char **refs)
{
	size_t c, slen, len;
	int allocated;
	char *qbuf, *qp;

	if(refs)
	{
		allocated = 0;
	}
	else
	{
		allocated = 1;
		refs = spindle_proxy_refs(spindle, from);
		if(!refs)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain references from <%s>\n", from);
			return -1;
		}
	}
	if(strlen(from) > strlen(to))
	{
		slen = strlen(from);
	}
	else
	{
		slen = strlen(to);
	}
	len = 80 + strlen(spindle->root);
	for(c = 0; refs[c]; c++)
	{
		len += strlen(refs[c]) +  slen + 24;
	}
	qbuf = (char *) calloc(1, len + 1);
	if(!qbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate SPARQL query buffer\n");
		if(allocated)
		{
			spindle_proxy_refs_destroy(refs);
		}
		return -1;
	}
	/* Generate an INSERT DATA for the new references */
	qp = qbuf;
	qp += sprintf(qp, "PREFIX owl: <http://www.w3.org/2002/07/owl#>\n"
				  "INSERT DATA {\n"
				  "GRAPH <%s> {\n", spindle->root);
	for(c = 0; refs[c]; c++)
	{
		qp += sprintf(qp, "<%s> owl:sameAs <%s> .\n", refs[c], to);
	}
	qp += sprintf(qp, "} }");
	sparql_update(spindle->sparql, qbuf, strlen(qbuf));
	/* Generate a DELETE DATA for the old references */
	qp = qbuf;
	qp += sprintf(qp, "PREFIX owl: <http://www.w3.org/2002/07/owl#>\n"
				  "DELETE DATA {\n"
				  "GRAPH <%s> {\n", spindle->root);
	for(c = 0; refs[c]; c++)
	{
		qp += sprintf(qp, "<%s> owl:sameAs <%s> .\n", refs[c], from);
	}
	qp += sprintf(qp, "} }");
	sparql_update(spindle->sparql, qbuf, strlen(qbuf));
	free(qbuf);
	if(allocated)
	{
		spindle_proxy_refs_destroy(refs);
	}
	return 0;
}

/* Obtain all of the outbound references from a proxy */
char **
spindle_proxy_refs(SPINDLE *spindle, const char *uri)
{
	char *qbuf;
	char **refs, **p;
	size_t l;
	SPARQLRES *res;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *ruri;
	size_t count, size;
	const char *str;

	refs = NULL;
	count = 0;
	size = 0;
	l = strlen(uri) + strlen(spindle->root) + 127;
	qbuf = (char *) calloc(1, l + 1);
	if(!qbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate SPARQL query string\n");
		return NULL;
	}
	snprintf(qbuf, l, "SELECT DISTINCT ?s FROM <%s> WHERE { ?s <http://www.w3.org/2002/07/owl#sameAs> <%s> . }", spindle->root, uri);
	res = sparql_query(spindle->sparql, qbuf, strlen(qbuf));
	if(!res)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": SPARQL query failed");
		free(qbuf);
		return NULL;
	}
	free(qbuf);
	while((row = sparqlres_next(res)))
	{
		node = sparqlrow_binding(row, 0);
		if(node && librdf_node_is_resource(node) && (ruri = librdf_node_get_uri(node)))
		{
			str = (const char *) librdf_uri_as_string(ruri);
			if(!str)
			{
				twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain string form of URI\n");
				continue;
			}
			if(count + 1 >= size)
			{
				p = (char **) realloc(refs, sizeof(char *) * (size + SET_BLOCKSIZE));
				if(!p)
				{
					twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to extend reference list\n");
					spindle_proxy_refs_destroy(refs);
					refs = NULL;
					break;
				}
				refs = p;
				size += SET_BLOCKSIZE;				
			}
			refs[count] = strdup(str);
			if(!refs[count])
			{
				twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate <%s>\n", str);
				spindle_proxy_refs_destroy(refs);
				refs = NULL;
				break;				
			}
			count++;
			refs[count] = NULL;
		}
	}
	sparqlres_destroy(res);
	return refs;
}

/* Destroy a list of references */
void
spindle_proxy_refs_destroy(char **refs)
{
	size_t c;

	if(!refs)
	{
		return;
	}
	for(c = 0; refs[c]; c++)
	{
		free(refs[c]);
	}
	free(refs);
}

/* Store a relationship between a proxy and a processed entity */
int
spindle_proxy_relate(SPINDLE *spindle, const char *remote, const char *local)
{
	size_t l;
	char *qbuf;
	int r;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": adding <%s> (remote) owl:sameAs <%s> (local)\n", remote, local);
	l = strlen(spindle->root) + strlen(remote) + strlen(local) + 127;
	qbuf = (char *) calloc(1, l + 1);
	if(!qbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate SPARQL query buffer\n");
		return -1;
	}
	snprintf(qbuf, l, "PREFIX owl: <http://www.w3.org/2002/07/owl#>\nINSERT DATA {\nGRAPH <%s> {\n<%s> owl:sameAs <%s> . } }", spindle->root, remote, local);
	twine_logf(LOG_DEBUG, "%s\n", qbuf);
	r = sparql_update(spindle->sparql, qbuf, strlen(qbuf));
	free(qbuf);
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": SPARQL INSERT DATA failed\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": INSERT succeeded\n");
	return 0;
}
