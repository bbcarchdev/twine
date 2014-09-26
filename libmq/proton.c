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
		return 1;
	}
	proton->sub = pn_messenger_subscribe(proton->messenger, uri);
	if(!proton->sub || pn_messenger_errno(proton->messenger))
	{
		return 1;
	}
	pn_messenger_set_incoming_window(proton->messenger, 1);
	return 0;
}

int
mq_proton_connect_send_(struct mq_proton_struct *proton, const char *uri)
{
	(void) uri;

	proton->messenger = pn_messenger(NULL);
	if(!proton->messenger)
	{
		return -1;
	}
	pn_messenger_start(proton->messenger);
	if(pn_messenger_errno(proton->messenger))
	{
		return 1;
	}
	pn_messenger_set_outgoing_window(proton->messenger, 1);
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
mq_proton_create_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	message->msg = pn_message();
	if(!message->msg)
	{
		return 1;
	}
	return 0;
}

int
mq_proton_next_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	if(!pn_messenger_incoming(proton->messenger))
	{
		pn_messenger_recv(proton->messenger, -1);
		if(pn_messenger_errno(proton->messenger))
		{
			return 1;
		}
		if(!pn_messenger_incoming(proton->messenger))
		{
			return 1;
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
		return 1;
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
	if(message->tracker)
	{
		pn_messenger_settle(proton->messenger, message->tracker, 0);
	}
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
	if(message->tracker)
	{
		pn_messenger_settle(proton->messenger, message->tracker, 0);
	}
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
	if(message->tracker)
	{
		pn_messenger_settle(proton->messenger, message->tracker, 0);
	}
	pn_message_free(message->msg);
	memset(message, 0, sizeof(struct mq_proton_message_struct));
	return 0;
}

int
mq_proton_message_free_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	if(!message->msg)
	{
		errno = EINVAL;
		return -1;
	}
	if(message->tracker)
	{
		pn_messenger_settle(proton->messenger, message->tracker, 0);
	}
	pn_message_free(message->msg);
	memset(message, 0, sizeof(struct mq_proton_message_struct));
	return 0;
}

int
mq_proton_message_set_type_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message, const char *type)
{
	(void) proton;
	
	return pn_message_set_content_type(message->msg, type);
}

const char *
mq_proton_message_type_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	return pn_message_get_content_type(message->msg);
}

int
mq_proton_message_set_subject_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message, const char *subject)
{
	(void) proton;
	
	return pn_message_set_content_type(message->msg, subject);
}

const char *
mq_proton_message_subject_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	return pn_message_get_subject(message->msg);
}

int
mq_proton_message_set_address_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message, const char *address)
{
	int r;

	(void) proton;
	
	r = pn_message_set_address(message->msg, address);
	if(!r)
	{
		message->addressed = 1;
	}
	return r;
}

const char *
mq_proton_message_address_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message)
{
	(void) proton;

	return pn_message_get_address(message->msg);
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

int
mq_proton_errcode_(struct mq_proton_struct *proton)
{
	return pn_messenger_errno(proton->messenger);
}

const char *
mq_proton_errmsg_(struct mq_proton_struct *proton, int errcode, char *buf, size_t buflen)   
{
	(void) errcode;
	(void) buf;
	(void) buflen;

	return pn_error_text(pn_messenger_error(proton->messenger));
}

int
mq_proton_message_add_bytes_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message, unsigned char *buf, size_t len)
{
	(void) proton;

	if(!message->body)
	{
		message->body = pn_message_body(message->msg);
		if(!message->body)
		{
			return 1;
		}
	}
	if(pn_data_put_binary(message->body, pn_bytes(len, (char *) buf)))
	{
		return 1;
	}
	return 0;
}

int
mq_proton_message_send_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message, const char *uri)
{
	if(!message->addressed)
	{
		if(pn_message_set_address(message->msg, uri))
		{
			return 1;
		}
	}
	if(pn_messenger_put(proton->messenger, message->msg))
	{
		return 1;
	}
	return 0;
}

int
mq_proton_deliver_(struct mq_proton_struct *proton)
{
	if(pn_messenger_send(proton->messenger, -1))
	{
		return 1;
	}
	return 0;
}

#endif /*WITH_LIBQPID_PROTON*/
