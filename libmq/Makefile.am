## libmq: A library for interacting with message queues
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

ACLOCAL_AMFLAGS = -I ../m4

lib_LTLIBRARIES = libmq.la

include_HEADERS = libmq.h

bin_PROGRAMS = mq-recv mq-send

EXTRA_DIST = libmq.pc.in libmq-uninstalled.pc.in

DISTCLEANFILES = libmq.pc libcluster-mq.pc

pkgconfigdir = $(libdir)/pkgconfig

noinst_DATA = libmq-uninstalled.pc

pkgconfig_DATA = libmq.pc

libmq_la_SOURCES = p_libmq.h \
	connection.c message.c proton.c

libmq_la_LDFLAGS = -avoid-version

mq_recv_SOURCES = mq-recv.c
mq_recv_LDADD = libmq.la

mq_send_SOURCES = mq-send.c
mq_send_LDADD = libmq.la
