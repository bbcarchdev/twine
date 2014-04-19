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

#define MQ_ERRBUF_LEN                  128

/* Create a connection for receiving messages from a queue */
MQ *
mq_connect_recv(const char *uri, const char *reserved1, const char *reserved2)
{
	MQ *mq;
	int e;

	(void) reserved1;
	(void) reserved2;

	mq = (MQ *) calloc(1, sizeof(MQ));
	if(!mq)
	{
		return NULL;
	}
	mq->uri = strdup(uri);
	if(!mq->uri)
	{
		SET_ERRNO(mq);
		return mq;
	}
	mq->kind = MQK_DISCONNECTED;
#ifdef WITH_LIBQPID_PROTON
	if(!strncmp(uri, "amqp:", 5) || !strncmp(uri, "amqps:", 6))
	{
		mq->type = MQT_PROTON;
		if((e = mq_proton_connect_recv_(&(mq->d.proton), uri)))
		{
			SET_ERROR(mq, e);
			return mq;
		}
		mq->kind = MQK_RECV;
		return mq;
	}
#endif
	SET_SYSERR(mq, EINVAL);
	return mq;
}

/* Close a connection */
int
mq_disconnect(MQ *connection)
{
	switch(connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		mq_proton_disconnect_(&(connection->d.proton));
#endif
	}
	free(connection->errmsg);
	free(connection->uri);
	free(connection);
	return 0;
}


/* Wait for the next message to arrive */
MQMESSAGE *
mq_next(MQ *connection)
{
	MQMESSAGE *message;
	int e;

	RESET_ERROR(connection);
	if(connection->kind != MQK_RECV)
	{
		SET_SYSERR(connection, EINVAL);
		return NULL;
	}
	message = (MQMESSAGE *) calloc(1, sizeof(MQMESSAGE));
	if(!message)
	{
		SET_ERRNO(connection);
		return NULL;
	}
	message->connection = connection;
	message->state = MQS_RECEIVED;
	switch(connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		if((e = mq_proton_next_(&(connection->d.proton), &(message->d.proton))))
		{
			SET_ERROR(connection, e);
			free(message);
			return NULL;
		}
		return message;		
#endif
	}
	SET_SYSERR(connection, EINVAL);
	free(message);
	return NULL;
}

/* Internal: Determine the error code for a connection */
int
mq_errcode_(MQ *connection)
{
	switch(connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		return mq_proton_errcode_(&(connection->d.proton));
#endif
	default:
		return -1;
	}
}

/* Obtain the error state for a connection */
int
mq_error(MQ *connection)
{
	return (connection->syserr || connection->errcode);
}

/* Obtain the error message for a connection */
const char *
mq_errmsg(MQ *connection)
{
	const char *p;

	if(!connection->errmsg)
	{
		connection->errmsg = (char *) malloc(MQ_ERRBUF_LEN);
		if(!connection->errmsg)
		{
			return "Memory allocation error obtaining error message";
		}
	}	
	connection->errmsg[0] = 0;
	if(connection->syserr)
	{
		strerror_r(connection->syserr, connection->errmsg, MQ_ERRBUF_LEN);
		return connection->errmsg;
	}
	if(connection->errcode)
	{
		switch(connection->type)
		{
#ifdef WITH_LIBQPID_PROTON
		case MQT_PROTON:
			if((p = mq_proton_errmsg_(&(connection->d.proton), connection->errcode, connection->errmsg, MQ_ERRBUF_LEN)))
			{
				return p;
			}
#endif
		default:
			snprintf(connection->errmsg, MQ_ERRBUF_LEN, "Unknown error #%d", connection->errcode);
			return connection->errmsg;
		}
	}
	return "Success";
}

