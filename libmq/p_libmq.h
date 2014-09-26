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

#ifndef P_LIBMQ_H_
# define P_LIBMQ_H_                     1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# ifdef WITH_LIBQPID_PROTON
#  include <proton/message.h>
#  include <proton/messenger.h>
# endif

# include "libmq.h"

typedef enum
{
	MQT_PROTON = 1
} MQTYPE;

typedef enum
{
	MQK_DISCONNECTED,
	MQK_SEND,
	MQK_RECV
} MQKIND;

typedef enum
{
	MQS_CREATED,
	MQS_RECEIVED
} MQSTATE;

# ifdef WITH_LIBQPID_PROTON
struct mq_proton_struct
{
	pn_messenger_t *messenger;
};

struct mq_proton_message_struct
{
	pn_message_t *msg;
	pn_tracker_t tracker;
	pn_data_t *body;
	pn_bytes_t bytes;
};
# endif

struct mq_connection_struct
{
	MQTYPE type;
	MQKIND kind;
	int syserr;
	int errcode;
	char *errmsg;
	char *uri;
	union
	{
		struct mq_proton_struct proton;
	} d;
};

struct mq_message_struct
{
	MQ *connection;
	MQSTATE state;
	union
	{
		struct mq_proton_message_struct proton;
	} d;
};

# define RESET_ERROR(conn) \
	conn->syserr = conn->errcode = 0;

# define SET_ERROR(conn, result) \
	if(result == -1) \
	{ \
	    conn->syserr = errno; \
	} \
	else \
	{ \
	    conn->syserr = 0; \
	    conn->errcode = mq_errcode_(conn); \
	}

# define SET_SYSERR(conn, value) \
	conn->syserr = errno = value;

# define SET_ERRNO(conn) \
	conn->syserr = errno;


int mq_errcode_(MQ *connection);

# ifdef WITH_LIBQPID_PROTON
int mq_proton_connect_recv_(struct mq_proton_struct *proton, const char *uri);
int mq_proton_disconnect_(struct mq_proton_struct *proton);
int mq_proton_next_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
int mq_proton_message_accept_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
int mq_proton_message_reject_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
int mq_proton_message_pass_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
const char *mq_proton_message_type_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
const char *mq_proton_message_subject_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
const char *mq_proton_message_address_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
const unsigned char *mq_proton_message_body_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
size_t mq_proton_message_len_(struct mq_proton_struct *proton, struct mq_proton_message_struct *message);
int mq_proton_errcode_(struct mq_proton_struct *proton);
const char *mq_proton_errmsg_(struct mq_proton_struct *proton, int errcode, char *buf, size_t buflen);
# endif

#endif /*!P_LIBMQ_H_*/

