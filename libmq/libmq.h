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

#ifndef LIBMQ_H_
# define LIBMQ_H_                       1
# include <stddef.h>

# undef BEGIN_DECLS_
# undef END_DECLS_
# ifdef __cplusplus
#  define BEGIN_DECLS_                  extern "C" {
#  define END_DECLS_                    }
# else
#  define BEGIN_DECLS_
#  define END_DECLS_
# endif

typedef struct mq_connection_struct MQ;
typedef struct mq_message_struct MQMESSAGE;

BEGIN_DECLS_

/* Create a connection for receiving messages from a queue */
MQ *mq_connect_recv(const char *uri, const char *reserved1, const char *reserved2);
/* Create a connection for sending messages to a queue */
MQ *mq_connect_send(const char *uri, const char *reserved1, const char *reserved2);
/* Close a connection */
int mq_disconnect(MQ *connection);
/* Wait for the next message to arrive */
MQMESSAGE *mq_next(MQ *connection);
/* Obtain the error state for a connection */
int mq_error(MQ *connection);
/* Obtain the error message for a connection */
const char *mq_errmsg(MQ *connection);

/* Create a message */
MQMESSAGE *mq_message_create(MQ *connection);
/* Free a created message */
int mq_message_free(MQMESSAGE *message);
/* Accept a message */
int mq_message_accept(MQMESSAGE *message);
/* Reject a message */
int mq_message_reject(MQMESSAGE *message);
/* Pass on a message */
int mq_message_pass(MQMESSAGE *message);
/* Return the content type of a message */
const char *mq_message_type(MQMESSAGE *message);
/* Return the message body */
const unsigned char *mq_message_body(MQMESSAGE *message);
/* Return the length of the message body */
size_t mq_message_len(MQMESSAGE *message);

END_DECLS_

#endif /*!LIBMQ_H_*/
