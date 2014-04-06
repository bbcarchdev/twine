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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libmq.h"

int
main(int argc, char **argv)
{
	MQ *connection;
	MQMESSAGE *msg;
	size_t len;

	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s URI\n", argv[0]);
		return 1;
	}
	connection = mq_connect_recv(argv[1], NULL, NULL);
	if(!connection)
	{
		fprintf(stderr, "%s: cannot connect to '%s'\n", argv[0], argv[1]);
		return 1;
	}
	if(mq_error(connection))
	{
		fprintf(stderr, "%s: cannot connect to '%s': %s\n", argv[0], argv[1], mq_errmsg(connection));
		mq_disconnect(connection);
		return 1;
	}
	printf("%s: waiting for messages\n", argv[0]);
	while(1)
	{
		msg = mq_next(connection);
		if(!msg)
		{
			fprintf(stderr, "%s: failed to obtain next message: %s\n", argv[0], mq_errmsg(connection));
			break;
		}
		len = mq_message_len(msg);
		printf("%s: received message; type='%s', length=%lu\n",
			   argv[0], mq_message_type(msg), (unsigned long) len);
		if(len && len != (size_t) -1)
		{
			printf("------------------------------------------------------------------------\n");
			fwrite(mq_message_body(msg), len, 1, stdout);
			printf("\n------------------------------------------------------------------------\n");
		}
		mq_message_accept(msg);
	}
	mq_disconnect(connection);
	return 0;
}

