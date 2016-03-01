/* Twine: Message queue handling
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

#include "p_libutils.h"

static MQ *messenger;
static char mq_uri[256];

static int utils_mq_init(const char *confkey);

int
utils_mq_init_recv(const char *confkey)
{
	int r;

	if((r = utils_mq_init(confkey)))
	{
		return r;
	}
	messenger = mq_connect_recv(mq_uri, NULL, NULL);
	if(!messenger)
	{
		twine_logf(LOG_CRIT, "failed to create message queue client for <%s>\n", mq_uri);
		return -1;
	}
	if(mq_error(messenger))
	{
		twine_logf(LOG_CRIT, "failed to establish message queue receiver: %s\n", mq_errmsg(messenger));
		mq_disconnect(messenger);
		return -1;
	}
	return 0;
}

int
utils_mq_init_send(const char *confkey)
{
	int r;

	if((r = utils_mq_init(confkey)))
	{
		return r;
	}
	messenger = mq_connect_send(mq_uri, NULL, NULL);
	if(!messenger)
	{
		twine_logf(LOG_CRIT, "failed to create message queue client for <%s>\n", mq_uri);
		return -1;
	}
	if(mq_error(messenger))
	{
		twine_logf(LOG_CRIT, "failed to establish message queue receiver: %s\n", mq_errmsg(messenger));
		mq_disconnect(messenger);
		return -1;
	}
	return 0;
}

const char *
utils_mq_uri(void)
{
	return mq_uri;
}

MQ *
utils_mq_messenger(void)
{
	return messenger;
}

static int
utils_mq_init(const char *confkey)
{
	size_t l;

	if(messenger)
	{
		return 0;
	}
	/* Try the application-specific configuration key first */
	if(confkey)
	{
		l = twine_config_get(confkey, NULL, mq_uri, sizeof(mq_uri));
	}
	else
	{
		l = 0;
	}
	if(!l || l > sizeof(mq_uri))
	{
		/* Try the global 'uri' key in the [twine] section */
		l = twine_config_get(DEFAULT_CONFIG_SECTION_NAME ":mq", NULL, mq_uri, sizeof(mq_uri));
	}
	if(!l || l > sizeof(mq_uri))
	{
		/* If that fails, try the global 'uri' key in the [mq] section */
		l = twine_config_get("mq:uri", NULL, mq_uri, sizeof(mq_uri));
		if(l)
		{
			twine_logf(LOG_NOTICE, "The [mq] configuration section has been deprecated; you should use mq=URI in the application-specific or common [%s] section instead\n", DEFAULT_CONFIG_SECTION_NAME);
		}
	}
	if(!l || l == (size_t) -1 || l > sizeof(mq_uri))
	{
		twine_logf(LOG_CRIT, "failed to determine message queue URI\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, "establishing connection to <%s>\n", mq_uri);
	return 0;
}

