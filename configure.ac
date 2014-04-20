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

AC_INIT([twine],m4_esyscmd([/bin/sh m4/get-version.sh]))
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC
BT_PROG_CC_WARN

LT_INIT

BT_ENABLE_POSIX
BT_ENABLE_XSI
BT_REQUIRE_PTHREAD
BT_REQUIRE_LIBDL
BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBRDF
BT_REQUIRE_LIBQPID_PROTON
BT_REQUIRE_LIBXML2
BT_REQUIRE_LIBXSLT
BT_REQUIRE_LIBSPARQLCLIENT_INCLUDED

BT_BUILD_DOCS

## Override the XML2HTML variable to use docbook-html5 XSLT
if test x"$build_docs" = x"yes" ; then
   XML2HTML="\$(XSLTPROC) --nonet --xinclude \
   		--param \"html.linksfile\" \"'\$(abs_top_srcdir)/docbook-html5/res-links.xml'\" \
		--param \"html.navfile\" \"'\$(abs_top_srcdir)/docbook-html5/res-nav.xml'\" \
		--param \"html.ie78css\" \"'http://bbcarchdev.github.io/painting-by-numbers/ie78.css'\" \
		-o \$@ \
		\$(top_srcdir)/docbook-html5/docbook-html5.xsl"
fi

XSLTFILES='$(top_srcdir)/docbook-html5/docbook-html5.xsl \
	$(top_srcdir)/docbook-html5/doc.xsl \
	$(top_srcdir)/docbook-html5/block.xsl \
	$(top_srcdir)/docbook-html5/inline.xsl \
	$(top_srcdir)/docbook-html5/toc.xsl \
	$(top_srcdir)/docbook-html5/res-links.xml \
	$(top_srcdir)/docbook-html5/res-nav.xml'

AC_SUBST([XML2HTML])
AC_SUBST([XSLTFILES])

BT_DEFINE_PREFIX

AC_CONFIG_FILES([Makefile
libsupport/Makefile
libmq/Makefile
libutils/Makefile
libtwine/Makefile
libtwine/libtwine.pc
libtwine/libtwine-uninstalled.pc
writer/Makefile
bridges/Makefile
processors/Makefile
init.d/Makefile
conf/Makefile
conf/twine.conf
docs/Makefile
m4/Makefile
docbook-html5/Makefile
])

AC_CONFIG_FILES([init.d/twine-writer:init.d/twine-writer.sh.in],[chmod +x init.d/twine-writer])

AC_OUTPUT