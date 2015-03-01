/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
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

#include "p_libs3client.h"

static int s3_header_sort_(const void *a, const void *b);

static char *
stradd(char *dest, const char *src)
{
	strcpy(dest, src);
	return strchr(dest, 0);
}


struct curl_slist *
s3_sign(const char *method, const char *resource, const char *access_key, const char *secret, struct curl_slist *headers)
{
	const char *type, *md5, *date, *adate, *hp;
	size_t len, amzlen, amzcount, c, l;
	char *t, *s, *buf, *amzbuf;
	char **amzhdr;
	struct curl_slist *p;
	unsigned char digest[20];
	unsigned digestlen;
	char *sigbuf;
	BIO *bmem, *b64;
	BUF_MEM *bptr;
	time_t now;
	struct tm tm;
	char datebuf[64];

	type = md5 = date = adate = NULL;
	len = strlen(method) + strlen(resource) + 2;
	amzcount = 0;
	for(p = headers; p; p = p->next)
	{
		if(!strncasecmp(p->data, "content-type: ", 14))
		{
			type = p->data + 14;
		}
		else if(!strncasecmp(p->data, "content-md5: ", 13))
		{
			md5 = p->data + 13;
		}
		else if(!strncasecmp(p->data, "date: ", 6))
		{
			date = p->data + 6;
		}
		else if(!strncasecmp(p->data, "x-amz-date: ", 12))
		{
			adate = p->data + 12;
		}
		else if(!strncasecmp(p->data, "x-amz-", 6))
		{
			amzlen += strlen(p->data) + 1;
			amzcount++;
		}
	}
	if(adate)
	{
		/* x-amz-date takes precedence over date */
		date = adate;
	}
	if(!type)
	{
		type = "";
	}
	if(!md5)
	{
		md5 = "";
	}
	if(!date)
	{
		now = time(NULL);
		gmtime_r(&now, &tm);
		strcpy(datebuf, "Date: ");
		strftime(&(datebuf[6]), 57, "%a, %d %b %Y %H:%M:%S GMT", &tm);		
		headers = curl_slist_append(headers, datebuf);
		date = &(datebuf[6]);
	}
	len += amzlen + strlen(type) + strlen(md5) + strlen(date) + 2;
	buf = (char *) calloc(1, len + 1);
	if(!buf)
	{
		return NULL;
	}
	t = stradd(buf, method);
	*t = '\n';
	t++;
	t = stradd(t, md5);
	*t = '\n';
	t++;
	t = stradd(t, type);
	*t = '\n';
	t++;
	t = stradd(t, date);
	*t = '\n';
	t++;
	if(amzcount)
	{
		/* Build an array of x-amz-* headers, excluding x-amz-date */
		amzhdr = (char **) calloc(amzcount, sizeof(char *));
		amzbuf = (char *) calloc(1, amzlen + 1);
		s = amzbuf;
		amzcount = 0;
		for(p = headers; p; p = p->next)
		{
			if(!strncasecmp(p->data, "x-amz-date: ", 12))
			{
				continue;
			}
			else if(!strncasecmp(p->data, "x-amz-", 6))
			{
				amzhdr[amzcount] = s;
				amzcount++;
				for(hp = p->data; *hp; hp++)
				{
					if(*hp == ':')
					{
						*s = ':';
						s++;
						hp++;
						while(isspace(*hp))
						{
							hp++;
						}
						strcpy(s, hp);
						s = strchr(s, 0);
						break;
					}
					*s = tolower(*hp);
					s++;
					*s = 0;
				}
				s++;
			}
		}
		qsort(amzhdr, amzcount, sizeof(char *), s3_header_sort_);
		for(c = 0; c < amzcount; c++)
		{			
			hp = strchr(amzhdr[c], ':');
			if(!hp)
			{
				continue;
			}
			l = hp - amzhdr[c];
			t = stradd(t, amzhdr[c]);
			while(c + 1 < amzcount && !strncmp(amzhdr[c + 1], amzhdr[c], l))
			{
				c++;
				*t = ',';
				t++;
				t = stradd(t, amzhdr[c] + l + 1);
			}
			*t = '\n';
			t++;
		}
		free(amzbuf);
		free(amzhdr);
	}
	t = stradd(t, resource);
	HMAC(EVP_sha1(), secret, strlen(secret), (unsigned char *) buf, strlen(buf), digest, &digestlen);
	
	free(buf);

	sigbuf = (char *) calloc(1, strlen(access_key) + 80);
	t = stradd(sigbuf, "Authorization: AWS ");
	t = stradd(t, access_key);
	*t = ':';
	t++;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, digest, digestlen);
	(void) BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);
	memcpy(t, bptr->data, bptr->length - 1);
	t[bptr->length - 1] = 0;
	headers = curl_slist_append(headers, sigbuf);
	BIO_free_all(b64);
	free(sigbuf);
	return headers;
}

static int
s3_header_sort_(const void *a, const void *b)
{
	const char **stra, **strb;

	stra = (const char **) a;
	strb = (const char **) b;

	return strcmp(*stra, *strb);
}
