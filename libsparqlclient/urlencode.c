/* Twine: URL encoding
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

#include "p_libsparqlclient.h"

/* This functionality is intended to be aligned with libcurl's curl_easy_escape(),
 * but operates in-place.
 *
 * NOTE: The functions here are NOT safe to use on EBCDIC systems (or indeed
 * any system where a-z, A-Z and 0-9 are not contiguous and in lexical order)
 */

/* Determine how many bytes are required (including null terminator) to
 * store the urlencoded form of a string
 */

size_t
sparql_urlencode_size_(const char *src)
{
	size_t l = 1;
	
	for(; src && *src; src++)
	{
		if((*src >= 'a' && *src <= 'z') ||
		   (*src >= 'A' && *src <= 'Z') ||
		   (*src >= '0' && *src <= '9') ||
		   *src == '-' ||
		   *src == '.' ||
		   *src == '_' ||
		   *src == '~')
		{
			l++;
		}
		else
		{
			l += 3;
		}
	}
	return l;
}

/* Determine how many bytes are required (including null terminator) to
 * store the urlencoded form of a string of a specified length
 */
size_t
sparql_urlencode_lsize_(const char *src, size_t srclen)
{
	size_t l = 1;
	
	for(; src && srclen; src++)
	{
		if((*src >= 'a' && *src <= 'z') ||
		   (*src >= 'A' && *src <= 'Z') ||
		   (*src >= '0' && *src <= '9') ||
		   *src == '-' ||
		   *src == '.' ||
		   *src == '_' ||
		   *src == '~')
		{
			l++;
		}
		else
		{
			l += 3;
		}
		srclen--;
	}
	return l;
}

/* urlencode a string, placing the result in the supplied buffer, which
 * must be at least as many bytes in capacity as the value returned by
 * twine_urlencode_size() on the same source string.
 */
int
sparql_urlencode_(const char *src, char *dest, size_t destlen)
{
	static const char *xdigit = "0123456789abcdef";

	int ch;

	if(!destlen)
	{
		errno = EINVAL;
		return -1;
	}
	for(; src && *src && destlen > 0; src++)
	{
		if((*src >= 'a' && *src <= 'z') ||
		   (*src >= 'A' && *src <= 'Z') ||
		   (*src >= '0' && *src <= '9') ||
		   *src == '-' ||
		   *src == '.' ||
		   *src == '_' ||
		   *src == '~')
		{
			*dest = *src;
			dest++;
			destlen--;
		}		
		else if(destlen < 4)
		{
			break;
		}
		else
		{
			ch = (unsigned char) *src;
			*dest = '%';
			dest++;
			destlen--;
			*dest = xdigit[ch >> 4];
			dest++;
			destlen--;
			*dest = xdigit[ch & 0x0f];
			dest++;
			destlen--;
		}
	}
	*dest = 0;
	return 0;
}

int
sparql_urlencode_l_(const char *src, size_t srclen, char *dest, size_t destlen)
{
	static const char *xdigit = "0123456789abcdef";

	int ch;

	if(!destlen)
	{
		errno = EINVAL;
		return -1;
	}
	for(; src && srclen && destlen > 0; src++)
	{
		if((*src >= 'a' && *src <= 'z') ||
		   (*src >= 'A' && *src <= 'Z') ||
		   (*src >= '0' && *src <= '9') ||
		   *src == '-' ||
		   *src == '.' ||
		   *src == '_' ||
		   *src == '~')
		{
			*dest = *src;
			dest++;
			destlen--;
		}		
		else if(destlen < 4)
		{
			break;
		}
		else
		{
			ch = (unsigned char) *src;
			*dest = '%';
			dest++;
			destlen--;
			*dest = xdigit[ch >> 4];
			dest++;
			destlen--;
			*dest = xdigit[ch & 0x0f];
			dest++;
			destlen--;
		}
		srclen--;
	}
	*dest = 0;
	return 0;
}

				
