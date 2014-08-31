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

static SPINDLE spindle;

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{
	memset(&spindle, 0, sizeof(SPINDLE));
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	spindle.world = twine_rdf_world();
	if(!spindle.world)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain librdf world\n");
		return -1;
	}
	spindle.root = twine_config_geta("spindle.graph", "http://localhost/");
	if(!spindle.root)
	{
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": local graph prefix is <%s>\n", spindle.root);
	spindle.sparql = twine_sparql_create();
	if(!spindle.sparql)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create SPARQL connection\n");
		free(spindle.root);
		spindle.root = NULL;
		return -1;
	}
	twine_postproc_register(PLUGIN_NAME, spindle_process, &spindle);
	return 0;
}
