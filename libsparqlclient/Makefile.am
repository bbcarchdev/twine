##  Twine: SPARQL client library
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
	@LIBCURL_LIBS@ @LIBCURL_LOCAL_LIBS@

AM_CPPFLAGS = @AM_CPPFLAGS@

include_HEADERS = libsparqlclient.h

lib_LTLIBRARIES = libsparqlclient.la

libsparqlclient_la_SOURCES = p_libsparqlclient.h libsparqlclient.h \
	connection.c update.c rest.c urlencode.c

libsparqlclient_la_LDFLAGS = -avoid-version

pkgconfig_DATA = libsparqlclient.pc
