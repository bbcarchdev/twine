/* Twine: Utilities
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

#ifndef P_LIBUTILS_H_
# define P_LIBUTILS_H_                  1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <unistd.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/types.h>

# include "libsupport.h"
# include "libtwine.h"
# include "libmq.h"

# include <curl/curl.h>
# include <librdf.h>

# include "libutils.h"

# define DEFAULT_CONFIG_SECTION_NAME    "twine"

#endif /*!P_LIBUTILS_H_*/
