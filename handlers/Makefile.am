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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libtwine -I$(top_srcdir)/libtwine \
	-I$(top_builddir)/libs3client -I$(top_srcdir)/libs3client \
	$(LIBJSONDATA_CPPFLAGS)

LIBS = $(top_builddir)/libtwine/libtwine.la

moduledir = $(libdir)/twine

if HAVE_LIBJSONDATA

anansi_module = anansi.la

endif

module_LTLIBRARIES = s3.la $(anansi_module)

s3_la_SOURCES = s3.c
s3_la_LDFLAGS = -no-undefined -module -avoid-version
s3_la_LIBADD = $(top_builddir)/libs3client/libs3client.la

if HAVE_LIBJSONDATA

anansi_la_SOURCES = anansi.c
anansi_la_LDFLAGS = -no-undefined -module -avoid-version
anansi_la_LIBADD = $(top_builddir)/libs3client/libs3client.la $(LIBJSONDATA_LOCAL_LIBS) $(LIBJSONDATA_LIBS)

endif
