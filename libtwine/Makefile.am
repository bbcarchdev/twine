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

pkgconfigdir = @libdir@/pkgconfig

LIBS = \
	@LIBRDF_LIBS@ @LIBRDF_LOCAL_LIBS@ \
	@LIBDL_LIBS@ @LIBDL_LOCAL_LIBS@

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libsparqlclient -I$(top_srcdir)/libsparqlclient

lib_LTLIBRARIES = libtwine.la

include_HEADERS = libtwine.h

libtwine_la_SOURCES = p_libtwine.h libtwine.h libtwine-internals.h \
	init.c plugin.c logging.c sparql.c rdf.c config.c

libtwine_la_LDFLAGS = -avoid-version

libtwine_la_LIBADD = $(top_builddir)/libsparqlclient/libsparqlclient.la

pkgconfig_DATA = libtwine.pc

noinst_DATA = libtwine-uninstalled.pc