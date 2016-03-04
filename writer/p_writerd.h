/* Twine: Writer daemon
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

#ifndef P_WRITERD_H_
# define P_WRITERD_H_                   1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <signal.h>
# include <curl/curl.h>

# include "libmq.h"
# include "libutils.h"
# include "libtwine-internal.h"

# define TWINE_APP_NAME                 "writerd"

int writerd_runloop(TWINE *context);
int writerd_exit(void);

#endif /*!P_WRITERD_H_*/
