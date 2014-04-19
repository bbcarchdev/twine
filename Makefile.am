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

XSLT = \
	$(top_srcdir)/docbook-html5/docbook-html5.xsl \
	$(top_srcdir)/docbook-html5/doc.xsl \
	$(top_srcdir)/docbook-html5/block.xsl \
	$(top_srcdir)/docbook-html5/inline.xsl \
	$(top_srcdir)/docbook-html5/toc.xsl

dist_noinst_DATA = \
	index.html twine.xml local.css \
	twine-writerd.8.html \
	twine-inject.8.html \
	twine.conf.5.html

if BUILD_DOCS

index.html: twine.xml $(XSLT) $(abs_top_srcdir)/docbook-html5/res-links.xml $(abs_top_srcdir)/docbook-html5/res-nav.xml
	$(XSLTPROC) --nonet --xinclude \
		--param "html.linksfile" "'$(abs_top_srcdir)/docbook-html5/res-links.xml'" \
		--param "html.navfile" "'$(abs_top_srcdir)/docbook-html5/res-nav.xml'" \
		--param "html.ie78css" "'http://bbcarchdev.github.io/painting-by-numbers/ie78.css'" \
		-o $@ \
		$(top_srcdir)/docbook-html5/docbook-html5.xsl \
		$<

twine-writerd.8.html: $(top_srcdir)/writer/twine-writerd.xml $(XSLT) $(abs_top_srcdir)/docbook-html5/res-links.xml $(abs_top_srcdir)/docbook-html5/res-nav.xml
	$(XSLTPROC) --nonet --xinclude \
		--param "html.linksfile" "'$(abs_top_srcdir)/docbook-html5/res-links.xml'" \
		--param "html.navfile" "'$(abs_top_srcdir)/docbook-html5/res-nav.xml'" \
		--param "html.ie78css" "'http://bbcarchdev.github.io/painting-by-numbers/ie78.css'" \
		-o $@ \
		$(top_srcdir)/docbook-html5/docbook-html5.xsl \
		$<

twine-inject.8.html: $(top_srcdir)/bridges/twine-inject.xml $(XSLT) $(abs_top_srcdir)/docbook-html5/res-links.xml $(abs_top_srcdir)/docbook-html5/res-nav.xml
	$(XSLTPROC) --nonet --xinclude \
		--param "html.linksfile" "'$(abs_top_srcdir)/docbook-html5/res-links.xml'" \
		--param "html.navfile" "'$(abs_top_srcdir)/docbook-html5/res-nav.xml'" \
		--param "html.ie78css" "'http://bbcarchdev.github.io/painting-by-numbers/ie78.css'" \
		-o $@ \
		$(top_srcdir)/docbook-html5/docbook-html5.xsl \
		$<

twine.conf.5.html: $(top_srcdir)/conf/twine.conf.xml $(XSLT) $(abs_top_srcdir)/docbook-html5/res-links.xml $(abs_top_srcdir)/docbook-html5/res-nav.xml
	$(XSLTPROC) --nonet --xinclude \
		--param "html.linksfile" "'$(abs_top_srcdir)/docbook-html5/res-links.xml'" \
		--param "html.navfile" "'$(abs_top_srcdir)/docbook-html5/res-nav.xml'" \
		--param "html.ie78css" "'http://bbcarchdev.github.io/painting-by-numbers/ie78.css'" \
		-o $@ \
		$(top_srcdir)/docbook-html5/docbook-html5.xsl \
		$<

endif
