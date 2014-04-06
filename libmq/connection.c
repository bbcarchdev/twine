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

/* Create a connection for receiving messages from a queue */
MQ *
mq_connect_recv(const char *uri, const char *reserved1, const char *reserved2)
{
	MQ *mq;

	(void) reserved1;
	(void) reserved2;

	mq = (MQ *) calloc(1, sizeof(MQ));
	if(!mq)
	{
		return NULL;
	}
#ifdef WITH_LIBQPID_PROTON
	if(!strncmp(uri, "amqp:", 5) || !strncmp(uri, "amqps:", 6))
	{
		if(mq_proton_connect_recv_(&(mq->d.proton), uri))
		{
			free(mq);
			return NULL;
		}
		mq->type = MQT_PROTON;
		return mq;
	}
#endif
	errno = EINVAL;
	return NULL;
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
	free(connection);
	return 0;
}


/* Wait for the next message to arrive */
MQMESSAGE *
mq_next(MQ *connection)
{
	MQMESSAGE *message;

	message = (MQMESSAGE *) calloc(1, sizeof(MQMESSAGE));
	if(!message)
	{
		return NULL;
	}
	message->connection = connection;
	switch(connection->type)
	{
#ifdef WITH_LIBQPID_PROTON
	case MQT_PROTON:
		if(mq_proton_next_(&(connection->d.proton), &(message->d.proton)))
		{
			free(message);
			return NULL;
		}
		return message;		
#endif
	}
	free(message);
	return NULL;
}
