/* Twine: Stand-alone import process
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

#include "p_twinecli.h"

static void twinecli_usage(void);
static int twinecli_init(int argc, char **argv);
static int twinecli_process_args(int argc, char **argv);
static int twinecli_plugin_config_cb(const char *key, const char *value, void *data);
static int twinecli_import(const char *type, const char *filename);

static const char *bulk_import = NULL, *bulk_import_file = NULL;

static struct twinecli_extmime_struct extmime[] = {
	{ "trig", "application/trig" },
	{ "nq", "application/n-quads" },
	{ "xml", "text/xml" },
	{ "ttl", "text/turtle" },
	{ "rdf", "application/rdf+xml" },
	{ "html", "text/html" },
	{ "txt", "text/plain" },
	{ "json", "application/json" },
	{ "nt", "application/n-triples" },
	{ NULL, NULL }
};

int
main(int argc, char **argv)
{
	int r;

	if(twinecli_init(argc, argv))
	{
		return 1;
	}
	r = twinecli_import(bulk_import, bulk_import_file);
	twine_cleanup_();
	return r ? 1 : 0;
}

static int
twinecli_init(int argc, char **argv)
{
	struct twine_configfn_struct configfn;
		
	/* Initialise libtwine */
	twine_init_(log_vprintf);
	configfn.config_get = config_get;
	configfn.config_geta = config_geta;
	configfn.config_get_int = config_get_int;
	configfn.config_get_bool = config_get_bool;
	configfn.config_get_all = config_get_all;
	twine_config_init_(&configfn);
	
	/* Initialise the utilities library */
	if(utils_init(argc, argv, 0))
	{
		return -1;
	}
	/* Apply default configuration */
	if(config_init(utils_config_defaults))
	{
		return -1;
	}
	/* Process command-line arguments */
	if(twinecli_process_args(argc, argv))
	{
		return -1;
	}
	/* Load the configuration file */
	if(config_load(NULL))
	{
		return -1;
	}
	/* Update logging configuration to use configuration file */
	log_set_use_config(1);

	/* Initialise libCURL */
	curl_global_init(CURL_GLOBAL_ALL);
	/* Set up the SPARQL interface */
	if(writerd_sparql_init())
	{
		return -1;
	}
	/* Load plug-ins */
	if(config_get_all(TWINE_APP_NAME, "module", twinecli_plugin_config_cb, NULL))
	{
		return -1;
	}
	return 0;
}

static void
twinecli_usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [FILE]\n"
			"\n"
			"OPTIONS is one or more of:\n"
			"  -h                   Print this notice and exit\n"
			"  -d                   Enable debug output to standard error\n"
			"  -c FILE              Specify path to configuration file\n"
			"  -t TYPE              Perform a bulk import of TYPE\n"
			"\n"
			"If FILE is not specified, input will be read from standard input.\n"
			"One of FILE or -t TYPE must be specified, but both can be specified together.\n",
			utils_progname);
}

static int
twinecli_process_args(int argc, char **argv)
{
	int c;
	size_t n;
	const char *t;

	while((c = getopt(argc, argv, "hc:dt:")) != -1)
	{
		switch(c)
		{
		case 'h':
			twinecli_usage();
			exit(0);
		case 'f':
			config_set(TWINE_APP_NAME ":detach", "0");
			break;
		case 'c':
			config_set("global:configFile", optarg);
			break;
		case 'd':
			config_set("log:level", "debug");
			config_set("log:stderr", "1");
			config_set("sparql:verbose", "1");
			config_set("s3:verbose", "1");
			break;
		case 't':
			if(bulk_import)
			{
				fprintf(stderr, "%s: cannot specify multiple import formats ('%s' versus '%s')\n", utils_progname, bulk_import, optarg);
				return -1;
			}
			bulk_import = optarg;
			break;
		default:
			twinecli_usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc == 1)
	{
		config_set("log:stderr", "1");
		if(!bulk_import)
		{
			t = strrchr(argv[0], '.');
			if(t)
			{
				t++;
				for(n = 0; extmime[n].ext; n++)
				{
					if(!strcasecmp(extmime[n].ext, t))
					{
						bulk_import = extmime[n].mime;
						break;
					}
				}
			}
		}
		if(!bulk_import)
		{
			fprintf(stderr, "%s: the MIME type of '%s' cannot be automatically determined; specify it with '-t TYPE'\n", utils_progname, argv[0]);
			return -1;
		}
		bulk_import_file = argv[0];
		argc--;
		argv++;
	}
	if(argc || (!bulk_import && !bulk_import_file))
	{
		/* There should not be any remaining command-line arguments */
		twinecli_usage();
		return -1;
	}
	return 0;
}

static int
twinecli_plugin_config_cb(const char *key, const char *value, void *data)
{
	(void) key;
	(void) data;

	if(twine_plugin_load_(value))
	{
		return -1;
	}
	return 0;
}

static int
twinecli_import(const char *type, const char *filename)
{
	FILE *f;
	unsigned char *buffer, *p;
	size_t buflen, bufsize;
	ssize_t r;

	if(filename)
	{
		f = fopen(filename, "rb");
		if(!f)
		{
			twine_logf(LOG_CRIT, "%s: %s\n", filename, strerror(errno));
			return -1;
		}
	}
	else
	{
		f = stdin;
	}
	if(twine_bulk_supported(type))
	{
		r = twine_bulk_import(type, f);
		if(filename)
		{
			fclose(f);
		}
		return (r ? -1 : 0);
	}
	if(!twine_plugin_supported(type))
	{
		twine_logf(LOG_CRIT, "no registered plug-in supports the MIME type '%s'\n", type);
		if(filename)
		{
			fclose(f);
		}
		return -1;
	}
	if(filename)
	{
		twine_logf(LOG_INFO, "performing bulk import of '%s' from '%s'\n", type, filename);
	}
	else
	{
		twine_logf(LOG_INFO, "performing bulk import of '%s' from standard input\n", type);
	}
	/* Read file into the buffer, extending as needed */
	buffer = NULL;
	bufsize = 0;
	buflen = 0;
	while(!feof(f))
	{
		if(bufsize - buflen < 1024)
		{
			p = (unsigned char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				twine_logf(LOG_CRIT, "failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				free(buffer);
				return -1;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, f);
		if(r < 0)
		{
			if(filename)
			{
				twine_logf(LOG_CRIT, "error reading from '%s': %s\n", filename, strerror(errno));
			}
			else
			{
				twine_logf(LOG_CRIT, "error reading from standard input: %s\n", strerror(errno));
			}
			free(buffer);
			return -1;
		}
		buflen += r;
		buffer[buflen] = 0;
	}
	r = twine_plugin_process(type, buffer, buflen, NULL);
	if(r)
	{
		twine_logf(LOG_CRIT, "failed to process input as '%s'\n", type);
	}
	else
	{
		twine_logf(LOG_NOTICE, "successfully imported data as '%s'\n", type);
	}
	free(buffer);
	if(filename)
	{
		fclose(f);
	}
	return (r ? -1 : 0);
}
