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
	-I$(top_builddir)/libutils -I$(top_srcdir)/libutils \
	-I$(top_builddir)/libtwine -I$(top_srcdir)/libtwine \
	-I$(top_builddir)/libsparqlclient -I$(top_srcdir)/libsparqlclient

bin_PROGRAMS = twine

twine_SOURCES = p_twinecli.h \
	twinecli.c

twine_LDADD = \
	$(top_builddir)/libutils/libutils.la \
	$(top_builddir)/libtwine/libtwine.la \
	@LIBMQ_LOCAL_LIBS@ @LIBMQ_LIBS@ \
	@LIBAWSCLIENT_LOCAL_LIBS@ @LIBAWSCLIENT_LIBS@
	
