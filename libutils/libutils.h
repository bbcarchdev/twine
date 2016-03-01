/* Twine: Utilities
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

#ifndef LIBUTILS_H_
# define LIBUTILS_H_                    1

# include <stdint.h>
# include "libmq.h"

/* The name of the process, defined as basename(argv[0]) */
const char *utils_progname;
/* Non-zero if the process is a daemon */
int utils_is_daemon;

/* Initialise libutils */
int utils_init(int argc, char **argv, int daemon);

/* Apply default configuration values */
int utils_config_defaults(void);

/* Daemonize the process */
pid_t utils_daemon(const char *configkey, const char *pidfile);

/* Interface with libmq */
int utils_mq_init_recv(const char *confkey);
int utils_mq_init_send(const char *confkey);
const char *utils_mq_uri(void);
MQ *utils_mq_messenger(void);

/* URL encoding */
size_t utils_urlencode_size(const char *src);
size_t utils_urlencode_lsize(const char *src, size_t srclen);
int utils_urlencode(const char *src, char *dest, size_t destlen);
int utils_urlencode_l(const char *src, size_t srclen, char *dest, size_t destlen);

#endif /*!LIBUTILS_H_*/
