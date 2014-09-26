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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport \
	-I$(top_builddir)/libutils -I$(top_srcdir)/libutils \
	-I$(top_builddir)/libtwine -I$(top_srcdir)/libtwine \
	-I$(top_builddir)/libsparqlclient -I$(top_srcdir)/libsparqlclient \
	-I$(top_builddir)/libmq -I$(top_srcdir)/libmq

sbin_PROGRAMS = twine-writerd

twine_writerd_SOURCES = writer/p_writerd.h \
	writerd.c sparql.c runloop.c

twine_writerd_LDADD = \
	$(top_builddir)/libutils/libutils.la \
	$(top_builddir)/libsupport/libsupport.la \
	$(top_builddir)/libtwine/libtwine.la \
	$(top_builddir)/libmq/libmq.la

EXTRA_DIST = twine-writerd.xml

dist_man8_MANS = twine-writerd.8

twine-writerd.8: $(srcdir)/twine-writerd.xml
	$(XML2MAN) $<
