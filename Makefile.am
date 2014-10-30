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

dist_noinst_DATA = \
	twine.xml twine-writerd.xml twine-inject.xml twine.conf.xml libtwine.xml

dist_twinehtdoc_DATA = index.html twine-writerd.8.html twine-inject.8.html \
	twine.conf.5.html libtwine.3.html \
	local.css

index.html: $(srcdir)/twine.xml $(XSLTFILES) $(NAV) $(LINKS)
	$(XML2HTML) $<

twine-writerd.8.html: $(srcdir)/twine-writerd.xml $(XSLTFILES) $(NAV) $(LINKS)
	$(XML2HTML) $<

twine-inject.8.html: $(srcdir)/twine-inject.xml $(XSLTFILES) $(NAV) $(LINKS)
	$(XML2HTML) $<

twine.conf.5.html: $(srcdir)/twine.conf.xml $(XSLTFILES) $(NAV) $(LINKS)
	$(XML2HTML) $<

libtwine.3.html: $(srcdir)/libtwine.xml $(XSLTFILES) $(NAV) $(LINKS)
	$(XML2HTML) $<
