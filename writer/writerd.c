/* Twine: Writer daemon
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

#include "p_writerd.h"

struct extmime {
	const char *ext;
	const char *mime;
};

static void writerd_usage(void);
static int writerd_init(int argc, char **argv);
static int writerd_process_args(int argc, char **argv);
static void writerd_signal(int sig);

static TWINE *twine;

int
main(int argc, char **argv)
{
	pid_t child;
	int detach;

	if(writerd_init(argc, argv))
	{
		return 1;
	}
	detach = twine_config_get_bool(TWINE_APP_NAME ":detach", 1);
	signal(SIGHUP, SIG_IGN);
	if(detach)
	{
		signal(SIGINT, SIG_IGN);
	}
	else
	{
		signal(SIGINT, writerd_signal);
	}
	signal(SIGALRM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTERM, writerd_signal);
	signal(SIGABRT, writerd_signal);
	if(detach)
	{	   
		child = twine_daemonize(twine, LOCALSTATEDIR "/run/twine-writerd.pid");
		if(child < 0)
		{
			return 1;
		}
		if(child > 0)
		{
			return 0;
		}
	}
	/* Execute the runloop */
	if(writerd_runloop(twine))
	{
		return 1;
	}
	twine_destroy(twine);
	return 0;
}

static int
writerd_init(int argc, char **argv)
{
	/* Initialise libtwine */
	twine = twine_create();
	if(!twine)
	{
		fprintf(stderr, "%s: failed to initialise Twine context\n", argv[0]);
		return -1;
	}
	/* Set the app name, which is used when reading configuration settings */
	twine_set_appname(twine, TWINE_APP_NAME);
	/* writerd needs plug-ins loaded to work */
	twine_set_plugins_enabled(twine, 1);
	/* twine-writerd is a daemon under normal operation */
	twine_set_daemon(twine, 0);
	/* We will participate in a cluster if configured to do so */
	twine_cluster_enable(twine, 1);
	/* Initialise the utilities library */
	if(utils_init(argc, argv, 1))
	{
		return -1;
	}
	/* Process command-line arguments */
	if(writerd_process_args(argc, argv))
	{
		return -1;
	}
	/* Update logging configuration to use configuration file */

	if(twine_ready(twine))
	{
		return -1;
	}
	/* Set up the AMQP interface */
	if(utils_mq_init_recv(TWINE_APP_NAME ":mq"))
	{
		return -1;
	}
	return 0;
}

static void
writerd_usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [FILE]\n"
			"\n"
			"OPTIONS is one or more of:\n"
			"  -h                   Print this notice and exit\n"
			"  -f                   Don't detach and run in the background\n"
			"  -d                   Enable debug output to standard error\n"
			"  -c FILE              Specify path to configuration file\n"
			"  -D SECTION:KEY       Set config option KEY in [SECTION] to 1\n"
			"  -D SECTION:KEY=VALUE Set config option KEY in [SECTION] to VALUE\n"
			"\n",
			utils_progname);
}

static int
writerd_process_args(int argc, char **argv)
{
	int c;
	char *p;

	while((c = getopt(argc, argv, "hfc:dt:D:")) != -1)
	{
		switch(c)
		{
		case 'h':
			writerd_usage();
			exit(0);
		case 'f':
			twine_config_set(TWINE_APP_NAME ":detach", "0");
			break;
		case 'c':
			twine_config_set("global:configFile", optarg);
			break;
		case 'd':
			twine_config_set("log:level", "debug");
			twine_config_set("log:stderr", "1");
			twine_config_set("sparql:verbose", "1");
			twine_config_set("s3:verbose", "1");
			twine_config_set(TWINE_APP_NAME ":detach", "0");
			break;
		case 'D':
		    p = strchr(optarg, '=');
			if(p)
			{
				*p = 0;
				p++;
				if(!strchr(optarg, ':'))
				{
					fprintf(stderr, "%s: configuration option must be specified as `section:key`=value\n", utils_progname);
					return -1;
				}
				twine_config_set(optarg, p);
			}
			else
			{
				if(!strchr(optarg, ':'))
				{
					fprintf(stderr, "%s: configuration option must be specified as `section:key`=value\n", utils_progname);
					return -1;
				}
				twine_config_set(optarg, "1");
			}
			break;
		default:
			writerd_usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc)
	{
		/* There should not be any remaining command-line arguments */
		writerd_usage();
		return -1;
	}
	return 0;
}

static void
writerd_signal(int signo)
{
	(void) signo;
	
	writerd_exit();
}
