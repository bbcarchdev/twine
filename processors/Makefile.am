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

SUBDIRS = spindle

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libtwine -I$(top_srcdir)/libtwine \
	-I$(top_builddir)/libs3client -I$(top_srcdir)/libs3client
LIBS = $(top_builddir)/libtwine/libtwine.la

moduledir = $(libdir)/twine

if HAVE_LIBJSONDATA

anansi_module = anansi.la

endif

module_LTLIBRARIES = rdf.la xslt.la geonames.la s3.la $(anansi_module)

rdf_la_SOURCES = rdf.c
rdf_la_LDFLAGS = -no-undefined -module -avoid-version

geonames_la_SOURCES = geonames.c
geonames_la_LDFLAGS = -no-undefined -module -avoid-version

s3_la_SOURCES = s3.c
s3_la_LDFLAGS = -no-undefined -module -avoid-version
s3_la_LIBADD = $(top_builddir)/libs3client/libs3client.la

if HAVE_LIBJSONDATA

anansi_la_SOURCES = anansi.c
anansi_la_LDFLAGS = -no-undefined -module -avoid-version
anansi_la_LIBADD = $(top_builddir)/libs3client/libs3client.la $(LIBJSONDATA_LOCAL_LIBS) $(LIBJSONDATA_LIBS)

endif

xslt_la_SOURCES = xslt.c
xslt_la_LIBADD = \
	@LIBXSLT_LIBS@ @LIBXSLT_LOCAL_LIBS@ \
	@LIBXML2_LIBS@ @LIBXML2_LOCAL_LIBS@
xslt_la_LDFLAGS = -no-undefined -module -avoid-version

dist_doc_DATA = example-xml.xml

dist_module_DATA = example-xml.xsl
