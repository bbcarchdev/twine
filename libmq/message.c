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

#include "p_libmq.h"

/* Accept and free a message */
int
mq_message_accept(MQMESSAGE *message)
{
	int e;

	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		if((e = mq_proton_message_accept_(&(message->connection->d.proton), &(message->d.proton))))
		{
			SET_ERROR(message->connection, e);
			free(message);
			return -1;
		}
		break;
#endif
	}
	free(message);
	return 0;	
}

/* Reject and free a message */
int
mq_message_reject(MQMESSAGE *message)
{
	int e;

	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		if((e = mq_proton_message_reject_(&(message->connection->d.proton), &(message->d.proton))))
		{
			SET_ERROR(message->connection, e);
			free(message);
			return -1;
		}
		break;
#endif
	}
	free(message);
	return 0;	
}

/* Pass on and free a message */
int
mq_message_pass(MQMESSAGE *message)
{
	int e;

	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		if((e = mq_proton_message_pass_(&(message->connection->d.proton), &(message->d.proton))))
		{
			SET_ERROR(message->connection, e);
			free(message);
			return -1;
		}
		break;
#endif
	}
	free(message);
	return 0;
}

/* Return the content type of a message */
const char *
mq_message_type(MQMESSAGE *message)
{	
	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_type_(&(message->connection->d.proton), &(message->d.proton));
#endif
	}
	return NULL;
}

/* Return the message body */
const unsigned char *
mq_message_body(MQMESSAGE *message)
{
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_body_(&(message->connection->d.proton), &(message->d.proton));
#endif
	}
	return NULL;
}

/* Return the length of the message body */
size_t
mq_message_len(MQMESSAGE *message)
{
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_len_(&(message->connection->d.proton), &(message->d.proton));
#endif
	}
	return (size_t) -1;
}
