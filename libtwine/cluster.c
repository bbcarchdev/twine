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

static int twine_cluster_balancer_(CLUSTER *cluster, CLUSTERSTATE *state);

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
	char *t;
	int i;
	
	if(context->cluster_enabled)
	{
		t = config_geta("*:cluster-name", "twine");
		if(!t)
		{
			twine_logf(LOG_CRIT, "failed to determine cluster name from configuration\n");
			return -1;
		}
		context->cluster = cluster_create(t);
		if(!context->cluster)
		{
			twine_logf(LOG_CRIT, "failed to create cluster object\n");
			free(t);
			return -1;
		}
		free(t);
		cluster_set_workers(context->cluster, 1);
		cluster_set_logger(context->cluster, twine_vlogf);
		cluster_set_balancer(context->cluster, twine_cluster_balancer_);
		if(twine_config_get_bool("*:cluster-verbose", 0))
		{
			cluster_set_verbose(context->cluster, 1);
		}
		if((t = twine_config_geta("*:environment", NULL)))
		{
			cluster_set_env(context->cluster, t);
			free(t);
		}
		if((t = config_geta("*:node-id", NULL)))
		{
			cluster_set_instance(context->cluster, t);
			free(t);
		}
		if((t = twine_config_geta("*:registry", NULL)))
		{
			cluster_set_registry(context->cluster, t);
			free(t);
		}
		else
		{
			i = twine_config_get_int("*:node-index", 0);
			cluster_static_set_index(context->cluster, i);
			i = twine_config_get_int("*:cluster-size", 1);
			cluster_static_set_total(context->cluster, i);
		}
	}
	else
	{
		context->cluster = cluster_create("twine");
		cluster_set_logger(context->cluster, twine_vlogf);
		cluster_set_workers(context->cluster, 1);
		cluster_static_set_index(context->cluster, 0);
		cluster_static_set_total(context->cluster, 1);
	}
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

/* Private: log changes to cluster state */
static int
twine_cluster_balancer_(CLUSTER *cluster, CLUSTERSTATE *state)
{
	(void) cluster;

	if(state->index == -1 || !state->total)
	{
		twine_logf(LOG_NOTICE, "cluster re-balanced: instance %s has left cluster %s/%s\n", cluster_instance(cluster), cluster_key(cluster), cluster_env(cluster));
	}
	else if(state->workers == 1)
	{
		twine_logf(LOG_NOTICE, "cluster re-balanced: instance %s single-thread index %d from cluster %s/%s of %d threads\n", cluster_instance(cluster), state->index + 1, cluster_key(cluster), cluster_env(cluster), state->total);
	}
	else
	{
		twine_logf(LOG_NOTICE, "cluster-re-balanced: instance %s thread indices %d..%d from cluster %s/%s of %d threads\n", cluster_instance(cluster), state->index + 1, state->index + state->workers, cluster_key(cluster), cluster_env(cluster), state->total);
	}
	return 0;
}
