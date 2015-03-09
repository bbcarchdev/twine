/* Twine: Read accepted resources from an Anansi queue database and pass them
 * to Twine for processing.
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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
#include <libsql.h>

#include "libsupport.h"
#include "libutils.h"
#include "libmq.h"

#define TWINE_APP_NAME                  "anansi-bridge"
#define ANANSI_URL_TYPE                 "application/x-anansi-url"

static void usage(void);
static int anansi_runloop(MQ *messenger, SQL *sql, const char *bucket, int fetch_all, int list_only, int no_complete);

int
main(int argc, char **argv)
{
	const char *dburi, *bucket;
	SQL *sql;
	MQ *messenger;
	int r, c, fetch_all, list_only, no_complete = 0;

	fetch_all = 0;
	list_only = 0;
	no_complete = 0;
	if(utils_init(argc, argv, 0))
	{
		return 1;
	}
	if(config_init(utils_config_defaults))
	{
		return 1;
	}
	while((c = getopt(argc, argv, "hdc:t:s:Aln")) != -1)
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
		case 'A':
			fetch_all = 1;
			break;
		case 'l':
			list_only = 1;
			break;
		case 'n':
			no_complete = 1;
			break;
		default:
			usage();
			return 1;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc)
	{
		usage();
		return 1;
	}
	if(config_load(NULL))
	{
		return 1;
	}
	if(utils_mq_init_send(TWINE_APP_NAME ":mq"))
	{
		return 1;
	}
	if(list_only)
	{
		messenger = NULL;
	}
	else
	{
		messenger = utils_mq_messenger();
		if(!messenger)
		{
			return 1;
		}
	}
	dburi = config_getptr_unlocked(TWINE_APP_NAME ":db", "mysql://localhost/anansi");
	sql = sql_connect(dburi);
	if(!sql)
	{
		log_printf(LOG_CRIT, "failed to connect to database <%s>\n", dburi);
		return 1;
	}
	bucket = config_getptr_unlocked(TWINE_APP_NAME ":bucket", "anansi");

	r = anansi_runloop(messenger, sql, bucket, fetch_all, list_only, no_complete);

	sql_disconnect(sql);

	if(!list_only)
	{
		mq_disconnect(messenger);
	}
	
	return (r ? 1 : 0);
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n"
			"\n"
			"OPTIONS is one or more of:\n\n"
			"  -h                   Print this notice and exit\n"
			"  -d                   Enable debug output\n"
			"  -A                   Fetch all entries, rather than only new ones\n"
			"  -l                   Write entry URLs to standard output instead of queueing\n"
			"  -n                   Don't mark entries as complete once queued\n"
			"\n",
			utils_progname);
}

static int 
anansi_runloop(MQ *messenger, SQL *sql, const char *bucket, int fetch_all, int list_only, int no_complete)
{
	MQMESSAGE *message;
	SQL_STATEMENT *rs;
	char *uribuf, *p;
	int printed, offset;

	uribuf = (char *) calloc(1, 8 + strlen(bucket) + 64);
	if(!uribuf)
	{
		log_printf(LOG_CRIT, "failed to allocate S3 URI buffer\n");
		return -1;
	}
	strcpy(uribuf, "s3://");
	strcat(uribuf, bucket);
	strcat(uribuf, "/");
	p = strchr(uribuf, 0);
	printed = 0;
	offset = 0;
	if(fetch_all && !no_complete)
	{
		log_printf(LOG_DEBUG, "marking previously-complete entries as new\n");
		sql_executef(sql, "UPDATE \"crawl_resource\" SET \"state\" = %Q WHERE \"state\" = %Q", "ACCEPTED", "COMPLETE");
	}
		
	for(;;)
	{
		if(fetch_all && no_complete)
		{
			rs = sql_queryf(sql, "SELECT \"hash\" FROM \"crawl_resource\" WHERE \"state\" IN (%Q, %Q) ORDER BY \"updated\" DESC LIMIT 5 OFFSET %d", "ACCEPTED", "COMPLETE", offset);
		}
		else
		{
			rs = sql_queryf(sql, "SELECT \"hash\" FROM \"crawl_resource\" WHERE \"state\" = %Q ORDER BY \"updated\" DESC LIMIT 5 OFFSET %d", "ACCEPTED", offset);
		}
		if(!rs)
		{
			log_printf(LOG_CRIT, "query for updated resources failed\n");
			return -1;
		}
		if(sql_stmt_eof(rs))
		{
			if(!printed)
			{
				if(list_only)
				{
					break;
				}
				log_printf(LOG_DEBUG, "no new resources remain; sleeping\n");
				printed = 1;
			}
			sql_stmt_destroy(rs);
			sleep(2);
			continue;
		}
		printed = 0;
		while(!sql_stmt_eof(rs))
		{
			if(sql_stmt_value(rs, 0, p, 64) > 64)
			{
				log_printf(LOG_CRIT, "unexpectedly large hash found\n");
				sql_stmt_destroy(rs);
				return -1;
			}
			if(list_only)
			{
				printf("%s\n", uribuf);
			}
			else
			{
				log_printf(LOG_DEBUG, "URI is <%s>\n", uribuf);
				if(!(message = mq_message_create(messenger)))
				{
					log_printf(LOG_CRIT, "failed to create new message\n");
					sql_stmt_destroy(rs);
					return -1;
				}
				mq_message_set_type(message, ANANSI_URL_TYPE);
				mq_message_add_bytes(message, (unsigned char *) uribuf, strlen(uribuf));
				if(mq_message_send(message))
				{
					log_printf(LOG_CRIT, "failed to send message: %s\n", mq_errmsg(messenger));
					sql_stmt_destroy(rs);
					return -1;
				}
			}
			if(no_complete)
			{
				offset++;
			}
			else
			{
				sql_executef(sql, "UPDATE \"crawl_resource\" SET \"state\" = %Q WHERE \"hash\" = %Q AND \"state\" = %Q", "COMPLETE", p, "ACCEPTED");
			}
			if(!list_only)
			{
				if(mq_deliver(messenger))
				{
					log_printf(LOG_CRIT, "failed to deliver message: %s\n", mq_errmsg(messenger));
					sql_stmt_destroy(rs);
					return -1;
				}
				mq_message_free(message);
			}
			sql_stmt_next(rs);
		}
		sql_stmt_destroy(rs);
	}
	return 0;
}

