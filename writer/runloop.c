/* Twine: Writer AMQP receive loop
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

#define AMQP_BUFSIZE                    1024

static int writerd_should_exit;

int
writerd_exit(void)
{
	log_printf(LOG_NOTICE, "received request to terminate\n");
	writerd_should_exit = 1;
	return 0;
}

int
writerd_runloop(void)
{
	MQ *messenger;
	MQMESSAGE *msg;
	const unsigned char *body;
	size_t len;
	const char *mime, *subject;
	
	messenger = utils_mq_messenger();
	if(!messenger)
	{
		log_printf(LOG_CRIT, "failed to create message queue conenction\n");
		return -1;
	}
	log_printf(LOG_NOTICE, TWINE_APP_NAME " ready and waiting for messages\n");
	while(!writerd_should_exit)
	{
		msg = mq_next(messenger);
		if(writerd_should_exit)
		{
			if(msg)
			{
				mq_message_reject(msg);
			}
			break;
		}
		if(!msg)
		{
			log_printf(LOG_CRIT, "failed to receive message: %s\n", mq_errmsg(messenger));
			return -1;
		}
		mime = mq_message_type(msg);
		subject = mq_message_subject(msg);
		body = mq_message_body(msg);
		len = mq_message_len(msg);
		if(!mime)
		{				
			log_printf(LOG_ERR, "rejecting message with no content type\n");
			mq_message_reject(msg);
			continue;
		}		 
		log_printf(LOG_DEBUG, "received a %s '%s' message via %s\n", mime, mq_message_subject(msg), mq_message_address(msg));
		if(twine_plugin_process(mime, body, len, subject))
		{
			log_printf(LOG_ERR, "processing of a %s '%s' message via %s failed\n", mime, mq_message_subject(msg), mq_message_address(msg));
			mq_message_reject(msg);
		}
		else
		{
			log_printf(LOG_INFO, "processing of a %s '%s' message via %s completed successfully\n", mime, mq_message_subject(msg), mq_message_address(msg));
			mq_message_accept(msg);
		}
	}
	log_printf(LOG_NOTICE, "shutting down\n");
	return 0;
}

 

