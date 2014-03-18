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
	pn_messenger_t *messenger;
	pn_tracker_t tracker;
	pn_message_t *msg;
	pn_data_t *body;
	pn_bytes_t bytes;
	const char *mime;

	messenger = utils_proton_messenger();
	if(!messenger)
	{
		log_printf(LOG_CRIT, "failed to create AMQP messenger\n");
		return -1;
	}
	msg = pn_message();
	if(!msg)
	{
		log_printf(LOG_CRIT, "failed to allocate memory for AMQP message\n");
		return -1;
	}
	log_printf(LOG_NOTICE, TWINE_APP_NAME " ready and waiting for messages\n");
	while(!writerd_should_exit)
	{
		pn_messenger_recv(messenger, AMQP_BUFSIZE);
		if(writerd_should_exit)
		{
			break;
		}
		if(pn_messenger_errno(messenger))
		{
			log_printf(LOG_CRIT, "%s\n", pn_error_text(pn_messenger_error(messenger)));
			return -1;
		}
		while(!writerd_should_exit && pn_messenger_incoming(messenger))
		{
			pn_messenger_get(messenger, msg);
			if(writerd_should_exit)
			{
				break;
			}
			if(pn_messenger_errno(messenger))
			{
				log_printf(LOG_CRIT, "%s\n", pn_error_text(pn_messenger_error(messenger)));
				return -1;
			}
			tracker = pn_messenger_incoming_tracker(messenger);
			body = pn_message_body(msg);
			pn_data_next(body);
			bytes = pn_data_get_binary(body);
			mime = pn_message_get_content_type(msg);
			if(!mime)
			{				
				log_printf(LOG_ERR, "rejecting message with no content type\n");
				pn_messenger_reject(messenger, tracker, 0);
				continue;
			}		 
			log_printf(LOG_DEBUG, "received a %s '%s' message via %s\n", mime, pn_message_get_subject(msg), pn_message_get_address(msg));
			if(twine_plugin_process_(mime, bytes.start, bytes.size))
			{
				log_printf(LOG_ERR, "processing of a %s '%s' message via %s failed\n", mime, pn_message_get_subject(msg), pn_message_get_address(msg));
				pn_messenger_reject(messenger, tracker, 0);
			}
			else
			{
				log_printf(LOG_ERR, "processing of a %s '%s' message via %s completed successfully\n", mime, pn_message_get_subject(msg), pn_message_get_address(msg));
				pn_messenger_accept(messenger, tracker, 0);
			}
		}
	}
	log_printf(LOG_NOTICE, "shutting down\n");
	pn_messenger_stop(messenger);
	pn_messenger_free(messenger);
	pn_message_free(msg);
	return 0;
}

 

