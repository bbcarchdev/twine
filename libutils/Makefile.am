##  Twine: A Linked Data workflow engine
##
##  Author: Mo McRoberts
##
##  Copyright (c) 2014-2016 BBC
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

## libutils is a static utility library for twine executables. It will be
## removed in the future and its core functionality rolled into libtwine.
##
## Different portions of libutils depend upon different external libraries,
## so some care ought to be taken in using different areas of functionality.
##
##     utils_config_*                   libsupport
##     utils_mq_*                       libmq
##

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libtwine -I$(top_srcdir)/libtwine \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport \
	-I$(top_builddir)/libmq -I$(top_srcdir)/libmq

noinst_LTLIBRARIES = libutils.la

libutils_la_SOURCES = p_libutils.h libutils.h \
	init.c urlencode.c daemon.c mq.c

libutils_la_LDFLAGS = -static

libutils_la_LIBADD = $(top_builddir)/libtwine/libtwine.la