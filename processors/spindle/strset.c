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

/* Create an empty string-set */
struct spindle_strset_struct *
spindle_strset_create(void)
{
	struct spindle_strset_struct *p;

	p = (struct spindle_strset_struct *) calloc(1, sizeof(struct spindle_strset_struct));
	if(!p)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for new string-set\n");
		return NULL;
	}
	return p;
}

/* Add a string to a string-set */
int
spindle_strset_add(struct spindle_strset_struct *set, const char *str)
{
	char **p;
	size_t c;

	for(c = 0; c < set->count; c++)
	{
		if(!strcmp(set->strings[c], str))
		{
			return 0;
		}
	}	
	if(set->count + 1 >= set->size)
	{
		p = (char **) realloc(set->strings, sizeof(char *) * (set->size + SET_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand string-set\n");
			return -1;
		}
		set->size += SET_BLOCKSIZE;
		set->strings = p;
	}
	set->strings[set->count] = strdup(str);
	if(!set->strings[set->count])
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate string\n");
		return -1;
	}
	set->count++;
	return 0;
}

/* Free the resources used by a string set */
int
spindle_strset_destroy(struct spindle_strset_struct *set)
{
	size_t c;

	for(c = 0; c < set->count; c++)
	{
		free(set->strings[c]);
	}
	free(set->strings);
	free(set);
	return 0;
}
