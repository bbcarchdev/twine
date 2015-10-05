/* Twine: Writer SPARQL interface
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

#include "p_writerd.h"

int
writerd_sparql_init(void)
{
	int verbose;
	char *query, *update, *data, *uri;

	verbose = config_get_bool("sparql:verbose", 0);
	uri = config_geta("sparql:uri", "http://localhost/");
	query = config_geta("sparql:query", NULL);
	update = config_geta("sparql:update", NULL);
	data = config_geta("sparql:data", NULL);
	twine_sparql_defaults_(uri, query, update, data, verbose);
	free(uri);
	free(query);
	free(update);
	free(data);
	return 0;
}
