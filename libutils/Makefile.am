##  Twine: A Linked Data workflow engine
##
##  Author: Mo McRoberts
##
##  Copyright (c) 2014 BBC
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.

## libutils is a static utility library for twine executables
##
## Different portions of libutils depend upon different external libraries,
## so some care ought to be taken in using different areas of functionality.
##
##     utils_config_*                   libsupport
##     utils_proton_*                   libsupport, libqpid-proton

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport

noinst_LTLIBRARIES = libutils.la

libutils_la_SOURCES = p_libutils.h libutils.h \
	init.c config.c proton.c urlencode.c daemon.c

libutils_la_LDFLAGS = -static
