/* Twine: Inject a file into the processing queue
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "libtwine-internal.h"
#include "libutils.h"
#include "libmq.h"

#define TWINE_APP_NAME                  "inject"

static void usage(void);

int
main(int argc, char **argv)
{
	TWINE *twine;
	MQ *messenger;
	MQMESSAGE *msg;
	const char *mime, *subj;
	char *buffer, *p, *subject;
	size_t bufsize, buflen;
	ssize_t r;
	int c;

	twine = twine_create();
	twine_set_appname(twine, TWINE_APP_NAME);
	if(utils_init(argc, argv, 0))
	{
		return 1;
	}
	mime = NULL;
	subj = NULL;
	while((c = getopt(argc, argv, "hdc:t:s:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			return 0;
		case 'd':
			twine_config_set("log:level", "debug");
			break;
		case 'c':
			twine_config_set("global:configFile", optarg);
			break;
		case 't':
			mime = optarg;
			break;
		case 's':
			subj = optarg;
			break;
		default:
			usage();
			return 1;
		}
	}
	if(!mime)
	{
		twine_logf(LOG_ERR, "no MIME type specified\n");
		usage();
		return 1;
	}
	if(twine_ready(twine))
	{
		return 1;
	}
	if(utils_mq_init_send(TWINE_APP_NAME ":mq"))
	{
		return 1;
	}	
	messenger = utils_mq_messenger();
	if(!messenger)
	{
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
				twine_logf(LOG_ERR, "failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				return 1;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, stdin);
		if(r < 0)
		{
			twine_logf(LOG_CRIT, "error reading from standard input: %s\n", strerror(errno));
			free(buffer);
			return -1;
		}
		buflen += r;	   
		buffer[buflen] = 0;
	}
	/* Wrap the buffer in a AMQP message and send it */
	msg = mq_message_create(messenger);
	if(subj)
	{
		subject = NULL;
	}
	else
	{
		subject = twine_config_geta(TWINE_APP_NAME ":subject", NULL);
		if(!subject)
		{
			subject = twine_config_geta("amqp:subject", NULL);
		}
		subj = subject;
	}
	mq_message_set_subject(msg, subj);
	mq_message_set_type(msg, mime);
	twine_logf(LOG_DEBUG, "sending %s message '%s' to <%s>\n", mime, subj, utils_mq_uri());
	mq_message_add_bytes(msg, (unsigned char *) buffer, buflen);
	mq_message_send(msg);

	/* Deliver any messages in the local queue */
	mq_deliver(messenger);

	/* Clean up and exit */
	mq_message_free(msg);
	free(subject);
	free(buffer);

	mq_disconnect(messenger);
	twine_destroy(twine);

	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] -t MIME-TYPE < FILE\n"
			"\n"
			"OPTIONS is one or more of:\n\n"
			"  -h                   Print this notice and exit\n"
			"  -d                   Enable debug output\n"
			"  -c FILE              Specify path to configuration file\n"
			"  -s SUBJECT           Specify a subject for the message\n"
			"\n",
			utils_progname);
}
