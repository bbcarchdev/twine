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

LIBS = \
	@LIBRDF_LIBS@ @LIBRDF_LOCAL_LIBS@ \
	@LIBDL_LIBS@ @LIBDL_LOCAL_LIBS@ \
	@LIBSPARQLCLIENT_LIBS@ @LIBSPARQLCLIENT_LOCAL_LIBS@ \
	@LIBCLUSTER_LIBS@ @LIBCLUSTER_LOCAL_LIBS@ \
	@PTHREAD_LIBS@ @PTHREAD_LOCAL_LIBS@ \
	@LIBSQL_LOCAL_LIBS@ @LIBSQL_LIBS@ 

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	@LIBSPARQLCLIENT_CPPFLAGS@ \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport

lib_LTLIBRARIES = libtwine.la

include_HEADERS = libtwine.h

libtwine_la_SOURCES = p_libtwine.h libtwine.h libtwine-internals.h \
	context.c plugin.c logging.c sparql.c rdf.c config.c mq.c \
	graph.c workflow.c daemon.c cluster.c legacy-api.c db-common.c \
	db-schema.c cache.c

libtwine_la_LDFLAGS = -avoid-version \
	-no-undefined \
	-export-symbols-regex '^twine_'

libtwine_la_LIBADD = $(top_builddir)/libsupport/libsupport.la \
	@LIBAWSCLIENT_LOCAL_LIBS@ @LIBAWSCLIENT_LIBS@

dist_man3_MANS = libtwine.3

libtwine.3: $(top_srcdir)/docs/libtwine.xml
	$(XML2MAN) $<
