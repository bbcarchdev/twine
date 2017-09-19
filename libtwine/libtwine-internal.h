/* Twine: Internal (application) interface
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

#ifndef LIBTWINE_INTERNAL_H_
# define LIBTWINE_INTERNAL_H_           1

# include <sys/types.h>

# include "libtwine.h"

# undef BEGIN_DECLS_
# undef END_DECLS_
# undef DEPRECATED_
# undef restrict
# ifdef __cplusplus
#  define BEGIN_DECLS_                  extern "C" {
#  define END_DECLS_                    }
# else
#  define BEGIN_DECLS_
#  define END_DECLS_
# endif
# ifdef __GNUC__
#  define DEPRECATED_                   __attribute__((deprecated))
# else
#  define DEPRECATED_
# endif
# ifdef __cplusplus
#  ifdef __GNUC__
#   define restrict                     __restrict__
#  else
#   define restrict
#  endif
# elif __STDC_VERSION__ < 199901L
#  define restrict
# endif

BEGIN_DECLS_

typedef void (*TWINELOGFN)(int prio, const char *fmt, va_list ap);

typedef struct twine_configfn_struct TWINECONFIGFNS;

struct twine_configfn_struct
{
	size_t (*config_get)(const char *key, const char *defval, char *buf, size_t bufsize);
	char *(*config_geta)(const char *key, const char *defval);
	int (*config_get_int)(const char *key, int defval);
	int (*config_get_bool)(const char *key, int defval);
	int (*config_get_all)(const char *section, const char *key, int (*fn)(const char *key, const char *value, void *data), void *data);
};

TWINE *twine_create(void);
int twine_destroy(TWINE *context);
int twine_ready(TWINE *context);
int twine_set_daemon(TWINE *context, int isdaemon);
int twine_set_logger(TWINE *context, TWINELOGFN logger);
int twine_set_config(TWINE *restrict context, TWINECONFIGFNS *restrict config);
int twine_set_appname(TWINE *restrict context, const char *name);
int twine_set_sparql(TWINE *restrict context, const char *base_uri, const char *query_uri, const char *update_uri, const char *data_uri, int verbose);
int twine_set_plugins_enabled(TWINE *context, int enabled);
void *twine_plugin_load(TWINE *restrict context, const char *restrict pathname);
int twine_plugin_unload(TWINE *restrict context, void *handle);
const char *twine_config_path(void);
int twine_set_job(TWINE *context, CLUSTERJOB *job);
pid_t twine_daemonize(TWINE *context, const char *pidfile);

/* Perform a bulk import from a file */
int twine_bulk(TWINE *context, const char *mimetype, FILE *file);

END_DECLS_

#endif /*!LIBTWINE_INTERNAL_H_*/
