/* Twine: Workflow processing
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

#include "p_libtwine.h"

/* Public: obtain the cluster object used by this context */
CLUSTER *
twine_cluster(TWINE *context)
{
	return context->cluster;
}

/* Internal: enable clustering support - if this is disabled (the default),
 * then a static single-node cluster object will be initialised by
 * twine_ready() instead.
 */
int
twine_cluster_enable(TWINE *context, int enabled)
{
	context->cluster_enabled = !!(enabled);
	return 0;
}

/* Private: initialise the cluster object */
int
twine_cluster_init_(TWINE *context)
{   
	context->cluster = cluster_create("twine");
	return 0;
}

/* Private: join the configured cluster */
int
twine_cluster_ready_(TWINE *context)
{
	cluster_set_workers(context->cluster, 1);
	cluster_static_set_index(context->cluster, 0);
	cluster_static_set_total(context->cluster, 1);
	if(cluster_join(context->cluster))
	{
		return -1;
	}
	return 0;
}

/* Private: release resources used by a cluster object */
int
twine_cluster_done_(TWINE *context)
{
	if(context->cluster)
	{
		cluster_leave(context->cluster);
		cluster_destroy(context->cluster);
	}
	return 0;
}
