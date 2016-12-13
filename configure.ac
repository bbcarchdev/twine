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

AC_INIT([twine],m4_esyscmd([/bin/sh m4/get-version.sh]),[RESFeedback@bbc.co.uk],,[https://bbcarchdev.github.io/twine/])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC([clang c99 gcc cc])
BT_PROG_CC_DEBUG
BT_PROG_CC_WARN
AC_PROG_CC_C99

LT_INIT

BT_ENABLE_POSIX_FULL
AC_SYS_LARGEFILE

BT_REQUIRE_PTHREAD
BT_REQUIRE_LIBDL

old_LIBS="$LIBS"
BT_CHECK_COMMONCRYPTO(,[
	BT_REQUIRE_OPENSSL
])
LIBS="$old_LIBS"

BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBRDF
BT_REQUIRE_LIBXML2
BT_REQUIRE_LIBXSLT
BT_REQUIRE_LIBCLUSTER
BT_REQUIRE_LIBSQL
BT_REQUIRE_LIBAWSCLIENT

dnl Check for these last in the event that they are not yet-built

BT_REQUIRE_LIBAWSCLIENT_INCLUDED([true])
BT_REQUIRE_LIBURI_INCLUDED([true])
BT_REQUIRE_LIBSPARQLCLIENT_INCLUDED
BT_REQUIRE_LIBMQ_INCLUDED

use_docbook_html5=yes
BT_BUILD_DOCS

XML2HTMLFLAGS="--param \"html.linksfile\" \"'file://\$(abs_top_srcdir)/docbook-html5/res-links.xml'\" \
  --param \"html.navfile\" \"'file://\$(abs_top_srcdir)/docbook-html5/res-nav.xml'\" \
  --param \"html.ie78css\" \"'//bbcarchdev.github.io/painting-by-numbers/ie78.css'\""
AC_SUBST([XML2HTMLFLAGS])

LINKS='${top_srcdir}/docbook-html5/res-links.xml'
AC_SUBST([LINKS])

NAV='${top_srcdir}/docbook-html5/res-nav.xml'
AC_SUBST([NAV])

twinehtdocdir='${docdir}'
AC_SUBST([twinehtdocdir])

BT_DEFINE_PREFIX

AC_CONFIG_FILES([Makefile
libtwine.pc
libtwine-uninstalled.pc
libsupport/Makefile
libutils/Makefile
libtwine/Makefile
writer/Makefile
cli/Makefile
bridges/Makefile
plug-ins/Makefile
plug-ins/rdf/Makefile
plug-ins/s3/Makefile
plug-ins/xslt/Makefile
plug-ins/geonames/Makefile
conf/Makefile
conf/twine.conf
docs/Makefile
m4/Makefile
docbook-html5/Makefile
])

AC_OUTPUT
