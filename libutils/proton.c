/* Twine: Proton AMQP handling
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

#include "p_libutils.h"

static pn_messenger_t *utils_amqp_messenger;
static char utils_amqp_uri[256];

static int utils_proton_init(const char *confkey);

int
utils_proton_init_recv(const char *confkey)
{
	int r;

	if((r = utils_proton_init(confkey)))
	{
		return r;
	}
	pn_messenger_subscribe(utils_amqp_messenger, utils_amqp_uri);
	if(pn_messenger_errno(utils_amqp_messenger))
	{
		log_printf(LOG_CRIT, "%s\n", pn_error_text(pn_messenger_error(utils_amqp_messenger)));
		return -1;
	}
	return 0;
}

int
utils_proton_init_send(const char *confkey)
{
	int r;

	if((r = utils_proton_init(confkey)))
	{
		return r;
	}
	return 0;
}

const char *
utils_proton_uri(void)
{
	return utils_amqp_uri;
}

pn_messenger_t *
utils_proton_messenger(void)
{
	return utils_amqp_messenger;
}

static int
utils_proton_init(const char *confkey)
{
	size_t l;

	if(utils_amqp_messenger)
	{
		return 0;
	}
	/* Try the application-specific configuration key first */
	if(confkey)
	{
		l = config_get(confkey, NULL, utils_amqp_uri, sizeof(utils_amqp_uri));
	}
	else
	{
		l = 0;
	}
	if(!l || l > sizeof(utils_amqp_uri))
	{
		/* If that fails, try the global 'uri' key in the [amqp] section */
		l = config_get("amqp:uri", NULL, utils_amqp_uri, sizeof(utils_amqp_uri));
	}
	if(!l || l == (size_t) -1 || l > sizeof(utils_amqp_uri))
	{
		log_printf(LOG_CRIT, "failed to determine AMQP URI\n");
		return -1;
	}
	log_printf(LOG_DEBUG, "establishing AMQP connection to <%s>\n", utils_amqp_uri);
	utils_amqp_messenger = pn_messenger(NULL);
	pn_messenger_start(utils_amqp_messenger);
	if(pn_messenger_errno(utils_amqp_messenger))
	{
		log_printf(LOG_CRIT, "%s\n", pn_error_text(pn_messenger_error(utils_amqp_messenger)));
		return -1;
	}
	return 0;
}

 

