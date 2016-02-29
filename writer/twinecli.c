/* Twine: Stand-alone import process
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

#include "p_twinecli.h"

static void twinecli_usage(void);
static int twinecli_init(int argc, char **argv);
static int twinecli_process_args(int argc, char **argv);
static int twinecli_import(const char *type, const char *filename);

static const char *bulk_import_type = NULL, *bulk_import_file = NULL;
static const char *cache_update_name = NULL, *cache_update_ident = NULL;
static int schema_update_only = 0;

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
	if(schema_update_only)
	{
		r = 0;
	}
	else
	{
		if(cache_update_name)
		{
			r = twine_update(cache_update_name, cache_update_ident);
		}
		else
		{
			r = twinecli_import(bulk_import_type, bulk_import_file);
		}
	}
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
	/* Perform final pre-flight checks */
	if(twine_preflight_())
	{
		return -1;
	}
	return 0;
}

static void
twinecli_usage(void)
{
	fprintf(stderr, "Usage:\n"
			"  %s [OPTIONS] [FILE]\n"
			"  %s [OPTIONS] -u NAME IDENTIFIER\n"
			"  %s [OPTIONS] -S\n"
			"\n"
			"OPTIONS is one or more of:\n"
			"  -h                   Print this notice and exit\n"
			"  -d                   Enable debug output to standard error\n"
			"  -c FILE              Specify path to configuration file\n"
			"  -t TYPE              Perform a bulk import of TYPE\n"
			"  -u NAME              Ask plug-in NAME to update IDENTIFIER\n"
			"  -D SECTION:KEY       Set config option KEY in [SECTION] to 1\n"
			"  -D SECTION:KEY=VALUE Set config option KEY in [SECTION] to VALUE\n"
			"  -S                   Perform schema migrations and then exit\n"
			"\n"
            "In the first usage form (bulk import):\n"			
			"  If FILE is not specified, input will be read from standard input.\n"
			"  One or both of FILE or -t TYPE must be specified.\n"
			"  The -u option cannot be used in this mode.\n"
			"In the second usage form (cache update):\n"
			"  This form asks the named plug-in to update its data about the resource\n"
			"  identified by IDENTIFIER. The format of IDENTIFIER is entirely specific\n"
			"  to the plug-in. The -t option cannot be used in this mode.\n"
			"In the third usage form (schema migrations):\n"
			"  Modules are initialised and database connections established, if\n"
			"  applicable. The process then shuts down immediately. None of the\n"
			"  -t, -u or FILE options may be specified.\n",
			utils_progname, utils_progname, utils_progname);
}

static int
twinecli_process_args(int argc, char **argv)
{
	int c;
	size_t n;
	const char *t;
	char *p;

	while((c = getopt(argc, argv, "hc:dt:u:D:S")) != -1)
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
				config_set(optarg, p);
			}
			else
			{
				if(!strchr(optarg, ':'))
				{
					fprintf(stderr, "%s: configuration option must be specified as `section:key`=value\n", utils_progname);
					return -1;
				}
				config_set(optarg, "1");
			}
			break;
		case 't':
			if(bulk_import_type)
			{
				fprintf(stderr, "%s: cannot specify multiple import formats ('%s' versus '%s')\n", utils_progname, bulk_import_type, optarg);
				return -1;
			}
			if(cache_update_name)
			{
				fprintf(stderr, "%s: cannot specify -t and -u options together\n", utils_progname);
				return -1;
			}
			bulk_import_type = optarg;
			break;
		case 'u':
			if(cache_update_name)
			{
				fprintf(stderr, "%s: the -u option cannot be specified multiple times ('%s' versus '%s')\n", utils_progname, cache_update_name, optarg);
				return -1;
			}
			if(bulk_import_type)
			{
				fprintf(stderr, "%s: cannot specify the -t and -u options together\n", utils_progname);
				return -1;
			}
			cache_update_name = optarg;
			break;
		case 'S':
			if(cache_update_name)
			{
				fprintf(stderr, "%s: warning: cannot specify the -S and -u options together\n", utils_progname);
				return -1;
			}
			if(bulk_import_type)
			{
				fprintf(stderr, "%s: warning: cannot specify the -S and -t options together\n", utils_progname);
				return -1;
			}
			schema_update_only = 1;
			break;
		default:
			twinecli_usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(schema_update_only)
	{
		/* When the -S option is provided, there should be no remaining
		 * arguments
		 */
		if(argc)
		{
			twinecli_usage();
			return -1;
		}
	}
	else if(cache_update_name)
	{
		/* There must be an identifier on the command-line when performing
		 * an update.
		 */
		if(!argc)
		{
			twinecli_usage();
			return -1;
		}
		cache_update_ident = argv[0];
		argc--;
		argv++;		
	}
	else if(argc)
	{
		/* Not performing a cache update.
		 *
		 * If there was a command-line argument, we can optionally try to
		 * infer the MIME type from the filename.
		 */
		if(!bulk_import_type)
		{		   
			/* Attempt to determine the MIME type from the filename */
			t = strrchr(argv[0], '.');
			if(t)
			{
				t++;
				for(n = 0; extmime[n].ext; n++)
				{
					if(!strcasecmp(extmime[n].ext, t))
					{
						bulk_import_type = extmime[n].mime;
						break;
					}
				}
			}
		}
		if(!bulk_import_type)
		{
			fprintf(stderr, "%s: the MIME type of '%s' cannot be automatically determined; specify it with '-t TYPE'\n", utils_progname, argv[0]);
			return -1;
		}
		bulk_import_file = argv[0];
		argc--;
		argv++;
	}
	else if(!bulk_import_type)
	{
		/* No arguments, no -t - implies stdin import, but a type needs
		 * to be specified for that
		 */
		twinecli_usage();
		return -1;
	}
	/* There should now be no remaining command-line arguments */
	if(argc)
	{
		twinecli_usage();
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
