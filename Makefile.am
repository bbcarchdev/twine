## Twine: A Linked Data workflow engine
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014-2015 BBC
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

ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = libsupport libmq libutils libtwine \
	libs3client \
	writer bridges handlers processors \
	conf docs m4 docbook-html5

SUBDIRS = libsupport @subdirs@ libmq libutils libtwine libs3client \
	writer bridges handlers processors \
	conf docs

pkgconfigdir = @libdir@/pkgconfig

pkgconfig_DATA = libtwine.pc

noinst_DATA = libtwine-uninstalled.pc

EXTRA_DIST = README.md LICENSE-2.0 libtwine.pc.in libtwine-uninstalled.pc.in
