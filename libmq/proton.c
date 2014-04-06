/* libmq: A library for interacting with message queues
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

#ifdef WITH_LIBQPID_PROTON

# include "p_libmq.h"

# define PROTON_BUFSIZE                 1024

int
mq_proton_connect_recv_(struct mq_proton_struct *proton, const char *uri)
{
	proton->messenger = pn_messenger(NULL);
	if(!proton->messenger)
	{
		return -1;
	}
	pn_messenger_start(proton->messenger);
	if(pn_messenger_errno(proton->messenger))
	{
		pn_messenger_free(proton->messenger);
		return -1;
	}
	pn_messenger_subscribe(proton->messenger, uri);
	if(pn_messenger_errno(proton->messenger))
	{
		pn_messenger_stop(proton->messenger);
		pn_messenger_free(proton->messenger);
		return -1;
	}
	return 0;
}

int
mq_proton_disconnect_(struct mq_proton_struct *proton)
{
	pn_messenger_stop(proton->messenger);
	pn_messenger_free(proton->messenger);
	return 0;
}

int
mq_proton_next_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	while(!pn_messenger_incoming(proton->messenger))
	{
		pn_messenger_recv(proton->messenger, PROTON_BUFSIZE);
		if(pn_messenger_errno(proton->messenger))
		{
			return -1;
		}
	}
	message->msg = pn_message();
	if(!message->msg)
	{
		return -1;
	}
	pn_messenger_get(proton->messenger, message->msg);
	if(pn_messenger_errno(proton->messenger))
	{
		pn_message_free(message->msg);
		return -1;
	}
	message->tracker = pn_messenger_incoming_tracker(proton->messenger);
	message->body = pn_message_body(message->msg);
	if(message->body)
	{
		pn_data_next(message->body);
		message->bytes = pn_data_get_binary(message->body);
	}
	return 0;
}

int
mq_proton_message_accept_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	if(!message->msg)
	{
		errno = EINVAL;
		return -1;
	}
	pn_messenger_accept(proton->messenger, message->tracker, 0);
	pn_message_free(message->msg);
	memset(message, 0, sizeof(struct mq_proton_message_struct));
	return 0;
}

int
mq_proton_message_reject_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	if(!message->msg)
	{
		errno = EINVAL;
		return -1;
	}
	pn_messenger_reject(proton->messenger, message->tracker, 0);
	pn_message_free(message->msg);
	memset(message, 0, sizeof(struct mq_proton_message_struct));
	return 0;
}

int
mq_proton_message_pass_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	if(!message->msg)
	{
		errno = EINVAL;
		return -1;
	}
	pn_message_free(message->msg);
	memset(message, 0, sizeof(struct mq_proton_message_struct));
	return 0;
}

const char *
mq_proton_message_type_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	return pn_message_get_content_type(message->msg);
}

const unsigned char *
mq_proton_message_body_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	if(message->body)
	{
		return (const unsigned char *) message->bytes.start;
	}
	errno = EINVAL;
	return NULL;
}

size_t
mq_proton_message_len_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	if(message->body)
	{
		return message->bytes.size;
	}
	errno = EINVAL;
	return (size_t) -1;
}

#endif /*WITH_LIBQPID_PROTON*/
