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
	if(message->state != MQS_RECEIVED)
	{
		SET_SYSERR(message->connection, EINVAL);
		return -1;
	}
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
	if(message->state != MQS_RECEIVED)
	{
		SET_SYSERR(message->connection, EINVAL);
		return -1;
	}
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
	if(message->state != MQS_RECEIVED)
	{
		SET_SYSERR(message->connection, EINVAL);
		return -1;
	}
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

/* Free an outgoing message */
int
mq_message_free(MQMESSAGE *message)
{
	int e;

	RESET_ERROR(message->connection);
	if(message->state != MQS_CREATED)
	{
		SET_SYSERR(message->connection, EINVAL);
		return -1;
	}
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		if((e = mq_proton_message_free_(&(message->connection->d.proton), &(message->d.proton))))
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


/* Set the content type of a message */
int
mq_message_set_type(MQMESSAGE *message, const char *type)
{	
	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_set_type_(&(message->connection->d.proton), &(message->d.proton), type);
#endif
	}
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

/* Set the content type of a message */
int
mq_message_set_subject(MQMESSAGE *message, const char *subject)
{	
	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_set_subject_(&(message->connection->d.proton), &(message->d.proton), subject);
#endif
	}
	return 0;
}

/* Return the subject of a message */
const char *
mq_message_subject(MQMESSAGE *message)
{
	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_subject_(&(message->connection->d.proton), &(message->d.proton));
#endif
	}
	return NULL;
}

/* Set the destination address of a message */
int
mq_message_set_address(MQMESSAGE *message, const char *address)
{	
	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_set_address_(&(message->connection->d.proton), &(message->d.proton), address);
#endif
	}
	return 0;
}

/* Return the address of a message */
const char *
mq_message_address(MQMESSAGE *message)
{
	RESET_ERROR(message->connection);
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_address_(&(message->connection->d.proton), &(message->d.proton));
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

/* Add bytes to the message body */
int
mq_message_add_bytes(MQMESSAGE *message, unsigned char *bytes, size_t len)
{	
	RESET_ERROR(message->connection);
	if(message->state != MQS_CREATED)
	{
		SET_SYSERR(message->connection, EINVAL);
		return -1;
	}
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_add_bytes_(&(message->connection->d.proton), &(message->d.proton), bytes, len);
#endif
	}
	return 0;
}

/* Send a message */
int
mq_message_send(MQMESSAGE *message)
{	
	RESET_ERROR(message->connection);
	if(message->state != MQS_CREATED)
	{
		SET_SYSERR(message->connection, EINVAL);
		return -1;
	}
	switch(message->connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_message_send_(&(message->connection->d.proton), &(message->d.proton), message->connection->uri);
#endif
	}
	return 0;
}
