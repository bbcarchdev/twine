##  Twine: A Linked Data workflow engine
##
##  Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libtwine -I$(top_srcdir)/libtwine

LIBS = $(top_builddir)/libtwine/libtwine.la

moduledir = $(libdir)/twine

module_LTLIBRARIES = geonames.la

geonames_la_SOURCES = geonames.c
geonames_la_LDFLAGS = -no-undefined -module -avoid-version
