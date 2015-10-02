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

AC_INIT([twine],m4_esyscmd([/bin/sh m4/get-version.sh]),[RESFeedback@bbc.co.uk],,[http://bbcarchdev.github.io/twine/])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC([clang c99 gcc cc])
BT_PROG_CC_WARN
BT_PROG_CC_DEBUG
AC_PROG_CC_C99

LT_INIT

BT_ENABLE_POSIX_FULL
AC_SYS_LARGEFILE

BT_REQUIRE_PTHREAD
BT_REQUIRE_LIBDL
BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBRDF
BT_REQUIRE_LIBQPID_PROTON
BT_REQUIRE_LIBXML2
BT_REQUIRE_LIBXSLT

BT_REQUIRE_LIBAWSCLIENT_INCLUDED([true])
BT_REQUIRE_LIBURI_INCLUDED([true])
BT_CHECK_LIBSQL([true])
BT_CHECK_LIBJSONDATA([true])

BT_REQUIRE_LIBSPARQLCLIENT_INCLUDED

AM_CONDITIONAL([HAVE_LIBJSONDATA],[test x"$have_libjsondata" = x"yes"])
AM_CONDITIONAL([HAVE_LIBSQL],[test x"$have_libsql" = x"yes"])

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
libmq/Makefile
libutils/Makefile
libtwine/Makefile
writer/Makefile
bridges/Makefile
handlers/Makefile
processors/Makefile
conf/Makefile
conf/twine.conf
docs/Makefile
m4/Makefile
docbook-html5/Makefile
])

AC_OUTPUT
