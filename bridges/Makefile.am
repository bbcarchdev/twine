## Twine: A Linked Data workflow engine
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014 BBC
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
	-I$(top_builddir)/libmq -I$(top_srcdir)/libmq

if HAVE_LIBSQL

sbin_PROGRAMS = twine-anansi-bridge

anansi_bridge = twine-anansi-bridge

endif

bin_PROGRAMS = twine-inject

twine_inject_SOURCES = inject.c

twine_inject_LDADD = \
	$(top_builddir)/libutils/libutils.la \
	$(top_builddir)/libsupport/libsupport.la \
	$(top_builddir)/libmq/libmq.la

if HAVE_LIBSQL

twine_anansi_bridge_SOURCES = anansi-bridge.c

twine_anansi_bridge_LDADD = \
	$(top_builddir)/libutils/libutils.la \
	$(top_builddir)/libsupport/libsupport.la \
	$(top_builddir)/libmq/libmq.la \
	$(LIBSQL_LOCAL_LIBS) $(LIBSQL_LIBS)

endif

EXTRA_DIST = twine-inject.xml

dist_man1_MANS = twine-inject.1

twine-inject.1: $(srcdir)/twine-inject.xml
	$(XML2MAN) $<
