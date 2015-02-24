##  Twine: A Linked Data workflow engine
##
##  Author: Mo McRoberts
##
##  Copyright (c) 2014-2015 BBC
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

EXTRA_DIST = README TODO proxy-data.ttl

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libtwine -I$(top_srcdir)/libtwine \
	-I$(top_builddir)/libsparqlclient -I$(top_srcdir)/libsparqlclient

LIBS = $(top_builddir)/libtwine/libtwine.la

moduledir = $(libdir)/twine

module_LTLIBRARIES = spindle.la

spindle_la_SOURCES = p_spindle.h \
	 module.c postproc.c proxy.c refset.c cache.c classes.c props.c strset.c

spindle_la_LIBADD = \
	@LIBUUID_LIBS@ @LIBUUID_LOCAL_LIBS@

spindle_la_LDFLAGS = -no-undefined -module -avoid-version

dist_module_DATA = rulebase.ttl
