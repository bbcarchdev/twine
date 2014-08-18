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

## These are the make rules for building this tree as part of the RES
## website - https://bbcarchdev.github.io/res/

top = ..
subdir = twine

include $(top)/config.mk

FILES = \
	index.html libtwine.3.html \
	local.css twine-masthead.png \
	twine-inject.8.html twine-writerd.8.html twine.conf.5.html

all: $(FILES)

clean:
	rm -f index.html

install:
	$(INSTALL) -m 755 -d $(DESTDIR)$(webdir)/$(PACKAGE)/$(subdir)
	for i in $(FILES) ; do $(INSTALL) -m 644 $$i $(DESTDIR)$(webdir)/$(PACKAGE)/$(subdir) ; done

index.html: twine.xml $(XSLT) $(LINKS) $(NAV)
	$(XML2HTML) $<
