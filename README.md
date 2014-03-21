# Twine

## What is Twine?

This is Twine, a framework for pushing RDF into quad-stores for later serving
as Linked Open Data (LOD).

Twine consists of:

* A queue-driven daemon (`twine-writerd`)
* 'Processor' modules which are baked into the writer daemon
* 'Bridge' components which feed data into the queues

The intention is that you will write your own processors and bridges to
support your particular application's needs.

Twine can use any AMQP 1.0-compliant message broker, and any
SPARQL 1.1-compliant quad-store. In development, we use Apache Qpid's C++
broker and 4store. Twine uses the Apache Qpid Proton messaging libraries
to speak to AMQP servers and libcurl to speak to quad-stores.

Twine is constructed with the assumption that the quad-store you use for your
RDF will contain one named graph per resource you intend to serve to consuming
applications. This means that retrieval can happen very quickly and easily,
but of course you can write processors which store data in a different way
if your prefer.

The role of a processor module is to receive a chunk of arbitrary data and
transform or interpret it such that the quad-store can be updated. A simple
case might be parsing a JSON or XML blob, creating some triples from that
blob, and then performing an HTTP PUT into a particular named graph.

Twine includes a simple processor module which can parse quads in either
N-Quads or TriG serialisations, and which are processed by iterating each
of the named graphs in them and pushing them into the quad-store.

The role of a bridge is to obtain change data from some source and submit
those changes as messages on the queue that the writer daemon listens on.

A simple bridge, `twine-inject`, is provided: this simply reads data from
a file and submits it as a message. You must specify the MIME type of the
file on the command-line, which must be a typed registered by a processor
module loaded by the writer daemon.

A more complex bridge might wait on a change-feed provided by another system,
receiving updates and continually submitting them to Twine for processing.

The rationale for splitting the processors and bridges (despite many
applications having a need for them to be tightly-coupled) is:

1. the processor may need to have more involved interaction with the
   quad-store than a simple push/replace operation, which wouldn't be
   appropriate for a bridge, as it shouldn't (and indeed may not be able
   to) access the quad-store directly.

2. it should be possible to push the same format of updates (i.e., content
   types) from multiple sources, resulting in consistent processing regardless
   of where the message came from.

## Building Twine from Git

Twine uses `autoconf`, `libtool` and `automake` to build. It also depends
upon `libcurl`, `libdl`, `libqpid-proton`, and `librdf`. Most modern Linux
distributions ship with development packages for these (especially `libcurl`
and `libdl`, the latter being part of `glibc`).

At the time of writing:

* You may find you need to build `libqpid-proton` from source, as it is not
  yet widely-packaged.

* In order for the `rdf` processor module (which accepts TriG or N-Quads
  messages and imports them into the quad-store directly) to work, you will
  need a version of `librdf` which parses quad-based formats into contexts.
  If version 1.0.18 or newer of `librdf` has been released, you should use
  that, otherwise you may need to fetch it directly from
  [GitHub](https://github.com/dajobe/librdf).

Once the dependencies are in place, you can check out and built Twine with:

```shell
$ git clone git://github.com/bbcarchdev/twine.git
$ cd twine
$ git submodule update --init --recursive
$ autoreconf -i
$ ./configure [--prefix=/opt/twine]
$ make
$ sudo make install
```