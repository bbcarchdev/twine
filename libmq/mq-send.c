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
#include <unistd.h>
#include <errno.h>

#include "libmq.h"

static const char *progname = "mq-send";

static void usage(void);

int
main(int argc, char **argv)
{
	MQ *connection;
	MQMESSAGE *msg;
	const char *type, *subj;
	char *buffer, *p;
	size_t bufsize, buflen;
	ssize_t r;
	int c;

	progname = argv[0];
	type = NULL;
	subj = NULL;
	while((c = getopt(argc, argv, "ht:s:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			return 0;
		case 't':
			type = optarg;
			break;
		case 's':
			subj = optarg;
			break;
		default:
			usage();
			return 1;
		}
	}	
	argc -= optind;
	argv += optind;
	if(!argc)
	{
		usage();
		return 1;
	}
	connection = mq_connect_send(argv[0], NULL, NULL);
	if(!connection)
	{
		fprintf(stderr, "%s: cannot connect to '%s'\n", progname, argv[0]);
		return 1;
	}
	if(mq_error(connection))
	{
		fprintf(stderr, "%s: cannot connect to '%s': %s\n", progname, argv[0], mq_errmsg(connection));
		mq_disconnect(connection);
		return 1;
	}
	/* Read stdin into the buffer, extending as needed */
	buffer = NULL;
	bufsize = 0;
	buflen = 0;
	while(!feof(stdin))
	{
		if(bufsize - buflen < 1024)
		{
			p = (char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				fprintf(stderr, "%s: failed to reallocate buffer from %u bytes to %u bytes\n", progname, (unsigned) bufsize, (unsigned) bufsize + 1024);
				return 1;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, stdin);
		if(r < 0)
		{
			fprintf(stderr, "%s: error reading from standard input: %s\n", progname, strerror(errno));
			free(buffer);
			return -1;
		}
		buflen += r;	   
		buffer[buflen] = 0;
	}
	/* Wrap the buffer in a message and send it */
	msg = mq_message_create(connection);
	if(subj)
	{
		mq_message_set_subject(msg, subj);
	}
	if(type)
	{	  
		mq_message_set_type(msg, type);
	}
	fprintf(stderr, "%s: sending %s message '%s' to <%s>\n", progname, type, subj, argv[0]);
	mq_message_add_bytes(msg, (unsigned char *) buffer, buflen);
	mq_message_send(msg);

	/* Deliver any messages in the local queue */
	mq_deliver(connection);

	/* Clean up and exit */
	mq_message_free(msg);
	free(buffer);
	mq_disconnect(connection);

	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] DEST-URI < FILE\n"
			"\n"
			"OPTIONS is one or more of:\n\n"
			"  -h                   Print this notice and exit\n"
			"  -t TYPE              Specify the message type\n"
			"  -s SUBJECT           Specify a subject for the message\n"
			"\n",
			progname);
}


