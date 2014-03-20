/* Twine: Inject a file into the processing queue
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libsupport.h"
#include "libutils.h"

#define TWINE_APP_NAME                  "inject"

static void usage(void);

int
main(int argc, char **argv)
{
	pn_messenger_t *messenger;
	pn_message_t *msg;
	pn_data_t *body;
	const char *mime;
	char *buffer, *p, *subject;
	size_t bufsize, buflen, r;
	int c;
	const char *uri;

	if(utils_init(argc, argv, 0))
	{
		return 1;
	}
	if(config_init(utils_config_defaults))
	{
		return 1;
	}
	mime = NULL;
	while((c = getopt(argc, argv, "hdc:t:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			return 0;
		case 'd':
			log_set_level(LOG_DEBUG);
			break;
		case 'c':
			config_set("global:configFile", optarg);
			break;
		case 't':
			mime = optarg;
			break;
		default:
			usage();
			return 1;
		}
	}
	if(!mime)
	{
		log_printf(LOG_ERR, "no MIME type specified\n");
		usage();
		return 1;
	}
	if(config_load(NULL))
	{
		return 1;
	}
	if(utils_proton_init_send(TWINE_APP_NAME ":amqp-uri"))
	{
		return 1;
	}	
	messenger = utils_proton_messenger();
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
				log_printf(LOG_ERR, "failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				return 1;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, stdin);
		buffer[r] = 0;
		buflen += r;
	}
	/* Wrap the buffer in a AMQP message and send it */
	uri = utils_proton_uri();
	msg = pn_message();
	subject = config_geta("inject:subject", NULL);
	if(!subject)
	{
		subject = config_geta("amqp:subject", NULL);
	}
	pn_message_set_address(msg, uri);
	pn_message_set_subject(msg, subject);
	pn_message_set_content_type(msg, mime);
	log_printf(LOG_DEBUG, "sending %s message '%s' to <%s>\n", mime, subject, uri);
	body = pn_message_body(msg);
	pn_data_put_binary(body, pn_bytes(buflen, buffer));
	pn_messenger_put(messenger, msg);

	/* Deliver any messages in the local queue */
	pn_messenger_send(messenger, -1);

	/* Clean up and exit */
	pn_message_free(msg);
	free(subject);
	free(buffer);

	/* XXX twine_proton_cleanup() */
	pn_messenger_stop(messenger);
	
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
		   "  -c FILE              Specify path to configuration file\n\n",
			utils_progname);
}
