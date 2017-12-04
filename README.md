# Twine
A workflow engine for processing [RDF](https://www.w3.org/RDF/) in customisable
ways.

[![Apache 2.0 licensed][license]](#license)

## Table of contents

* [Summary](#summary)
* [Plug-ins](#plug-ins)
* [History](#history)
* [Requirements](#requirements)
* [Building from source](#building-from-source)
* [Source tree structure](#source-tree-structure)
* [Changes as of Twine 7.x](#changes)
* [Contributing](#contributing)
* [License](#license)

## Summary

Twine is a harness for running RDF processing modules. Non-trivial use of Twine
depends upon [plug-ins](#plug-ins), which can register two primary types of
module: _[input handlers](#input-handlers)_ which take incoming data and convert
them to an in-memory RDF graph model; and _[processors](#processors),_ which
transform and/or export that model. All of the processing steps operate on sets
of RDF graphs. A linear chain of one or more such processors constitutes a
_[workflow](#workflows)._ Inputs are invoked as needed, with data taken either
from a Twine-managed _[message queue](#message-queue),_ or if using the Twine
CLI, read from a file or `stdin`. Finally, there are also _update_ modules,
however these are currently undocumented.

### Input handlers

Which input module is used to interpret an incoming message is determined by the
message's associated MIME type. Upon loading, each plug-in may register one or
more MIME types it can handle, and, when invoked, produces a data object that
gets sent to the workflow. The message may contain anything, such as a direct
RDF serialisation, locative information describing where some RDF data can be
obtained, or some other flag that the input module can interpret so as to be
able to provide the RDF.

Plug-ins are also able to register _bulk input_ modules. If handling a given
MIME type can result in multiple RDF graphs, a plug-in should register a bulk
input module rather than a regular input module to parse those source data (see
the `geonames` plug-in for an example). Each resultant graph is then sent
through the _workflow_ independently.

### Processors

Processors are the heart of the Twine system. Use processors to add or remove
triples from a graph, make inferences on the data (based on what's already in
the graph, or from external cues), export the data to storage, use it as input
to other executables, or perform any other activity. There are two built-in
processors which are always available, `sparql-get` and `sparql-put`, which read
from and write to the SPARQL store described in the active configuration file.
Note that `sparql-get` is a processor, and as such, does not read from the
message queue. It will discard the results of any previous processing and
replace them with the RDF returned by the SPARQL store.

### Workflows

A workflow is the ordered list of processors that some data will pass through —
the data object being passed from one processor callback to the next until one
fails or the final one succeeds.
A workflow is defined by the `workflow` configuration setting in the active
Twine configuration file, which names and sequences the processors to be used by
that Twine instance. To support multiple workflows, you need to run multiple
instances of Twine.

### Message queue

Messages are inserted into the input queue either during process invocation, by
Twine plug-ins, or from outside via inter-process communication. The [Advanced
Message Queuing Protocol](http://www.amqp.org/) is used. Each invocation of
Twine contains its own message queue.

## Plug-ins

As mentioned, plug-ins can perform various different functions within Twine. In
principle, they can perform any computation upon some RDF data that one might
conceive of. However, typically, they will:

* Handle different input formats, parsing and translating source data into
Twine's RDF model so that it can be further processed or stored (e.g., the
`xslt` plug-in).
* Deal with indirection, where messages don't contain actual source data, but
instead the location where the data can be found (e.g., the `s3` plug-in). Such
plug-ins can then parse the source data themselves; synchronously invoke Twine's
message handler allowing another plug-in to parse the data (as if the data had
just been pulled from the message queue — the `s3` plug-in does this); or push
it back onto the message queue for asynchronous processing.
* Perform transformation on RDF data (such as stripping triples out, or
generating new ones) —
[Spindle Strip](https://github.com/bbcarchdev/spindle/tree/develop/strip) is an
example of a simple plug-in which does this.
* Storing the resultant RDF data somewhere, via [SPARQL
Update](https://www.w3.org/Submission/SPARQL-Update/) or some other means (e.g.,
the built-in `sparql-put` processor).

Versatility is maximised when each module only performs one such task. A plug-in
should register multiple modules and arrange them into a _workflow_ to meet
complex needs.

Twine comes with four plug-ins.

### rdf

The `rdf` plug-in parses messages containing RDF serialised as TriG or N-Quads.
The resulting graphs are then available for transformation by other modules, or
simply for storing (for example via the built-in `sparql-put` processor). It
also registers a processor module called `dump-nquads`, which writes
N-Quads-serialised RDF to `stdout`.

### s3

The `s3` plug-in registers a single input module which handles messages with a
type of `application/x-s3-url`, which are expected to contain a single URI in
the form `s3://BUCKETNAME/RESOURCEKEY`. This URI is resolved to a HTTP URL using
the S3 endpoint and credentials specified in the Twine configuration file, which
is then fetched and the response body passed as-is back to Twine for processing,
as though the S3 resource's contents had been present in the message queue.

### xslt

The `xslt` plug-in is a configurable input module which applies XSLT stylesheets
to transform generic XML documents into RDF/XML, which Twine natively handles.
The plug-in registers itself as being able to handle MIME types specified in the
`[xslt]` section of the Twine configuration file, and applies the corresponding
stylesheet.

An example stylesheet and source XML document is provided as part of the
plug-in.

### geonames

The `geonames` plug-in accepts data in the GeoNames RDF dump format, which
consists of a repeating sequence of the following, each separated by newlines:

* Geonames URL (e.g., `http://sws.geonames.org/3/`)
* RDF/XML-formatted data for this graph (with any newlines stripped out)

For each pair, the plug-in simply reads the Geonames URL and transforms it into
a graph URI by appending `about.rdf`, then reads the data on the next line into
a named graph. The resulting models are pushed one-at-a-time into Twine for
onward processing or storage.

With the `geonames` and `rdf` plug-ins both loaded, a GeoNames dump can be
converted to N-Quads with:

```
$ twine -D twine:workflow=dump-nquads -t text/x-geonames-dump all-geonames-rdf.txt > geonames.nq
```

## History

Twine was originally written to receive data in multiple formats, and with
a small amount of format-specific code (or an XSLT stylesheet), push an RDF
representation of that data into a graph store via SPARQL. It can still be
used for this purpose via the `sparql-put` workflow processor.

## Requirements

Twine requires the following common libraries which are available either
out-of-the-box, or can be installed using native package management, on most
Unix-like operating systems:

* POSIX threads and libdl (generally shipped as part of the core C libraries on
most Unix-like systems)
* [Apple
CommonCrypto](https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/Common%20Crypto.3cc.html#//apple_ref/doc/man/3cc/CommonCrypto)
or [OpenSSL](https://www.openssl.org/)
* [cURL](https://curl.haxx.se/)
* [libxml2](http://www.xmlsoft.org/) and [libxslt](http://xmlsoft.org/XSLT/)
* [librdf](http://librdf.org/)

Note that in order to process source data in TriG or N-Quads formats correctly,
you must use an up-to-date version of librdf. If you have an older version
installed on your system, Twine will compile correctly, but will potentially not
be able to locate any named graphs in the parsed RDF quads when attempting to
import them.

Twine also requires the following libraries which are available from the [BBC
Archive Development Github account](https://github.com/bbcarchdev):

* [libcluster](https://github.com/bbcarchdev/libcluster)
* [libmq](https://github.com/bbcarchdev/libmq)
* [libawsclient](https://github.com/bbcarchdev/libawsclient)
* [liburi](https://github.com/bbcarchdev/liburi)
* [libsparqlclient](https://github.com/bbcarchdev/libsparqlclient)

Finally, building Twine requires working GNU autotools (`make`, `autoconf`,
`automake`, `libtool`, and so on) as well as a C compiler and your operating
system’s usual developer packages.

## Building from source

If building from a Git clone, you must first run:

	$ git submodule update --init --recursive
	$ autoreconf -i

Then, you can configure and build Twine in the usual way:

	$ ./configure --prefix=/opt/twine
	$ make
	$ sudo make install

Use `./configure --help` to see a list of available `configure` options. As
an Automake-based project, Twine supports building in a separate directory from
the sources and installing to a staging area (via
`make install DESTDIR=/some/path`).

## Source tree structure

* `libtwine` contains the core of the workflow engine, and is linked against
by both tools and daemons which perform processing and also any loadable
plug-in modules which provide different kinds of processing functionality.
* `cli` contains the command-line processor, installed as
`${exec_prefix}/bin/twine`. This processor applies a configured workflow to
some input file in a synchronous fashion.
* `writer` contains the queue-driven processing daemon, `twine-writerd`, which
performs the same job as the command-line processor, except that it
continuously reads messages from a queue and processes those, instead of
reading from a file supplied on the command-line or via standard input.
* `bridges` currently contains a single example 'bridge', `twine-inject`,
which can be used as a basis for developing others and is useful for testing;
a bridge is a mechanism for feeding messages into a message queue that can be
processed by `twine-writerd`. `twine-inject` simply posts an arbitrary
message, of a MIME type specified on the command-line, whose contents
are read from standard input. No validation is performed by the bridge itself.
* `plug-ins` contains modules which can be configured to be loaded by Twine.
Twine plug-ins are responsible for fetching, parsing and transforming data,
and optionally for writing the result to files, an RDF store, or some other
kind of database. `libtwine` includes built-in plug-ins for fetching
previously-stored RDF via SPARQL (so that a processing plug-in can compare
new and old data), and for writing RDF back to a store via SPARQL Update.
* `libsupport` contains support for handling configuration files and logging
and is used by (and built into) `libtwine`
* `libutils` contains some supporting code which will over time be merged into
`libtwine`

## Changes

Significant changes as of Twine 7.x:—

### Configuration file structure

The configuration file structure has been simplified, although
older settings will be used (emitting a warning) if they are present.

The majority of Twine’s configuration is now in the `[twine]` section, whose
options apply to both the `twine` command-line utility, the `twine-writerd`
daemon and the `twine-inject` tool. Individual options can be overridden by
adding them to specific `[twine-cli]`, `[writerd]` or `[inject]` sections.
Plug-ins will continue to use their own sections.

See the [configuration file
example](https://github.com/bbcarchdev/twine/blob/develop/conf/twine.conf.in)
for further details.

### Configurable workflows

Earlier versions of Twine applied a fixed workflow to an RDF graph being
processed, which consisted of:—

* Fetching a copy of the existing data from the configured SPARQL store
* Invoking any pre-processors
* Replacing the data with the version of the graph in the SPARQL store
* Invoking any post-processors

As of Twine 7.x, pre- and post-processors have been deprecated, with module
authors encouraged to implement generic graph processors instead. The
fixed in-built workflow is now customisable in the configuration file, although
the defaults are such that Twine will continue to apply the workflow described
above until configured not to.

A workflow configuration consists simply of a list of comma-separated graph
processors (usually registered by plug-ins) to apply to ingested RDF data.
By default, this consists of the following in-built processors:—

* `sparql-get`: fetch a copy of existing data from the SPARQL store
* `deprecated:preprocess`: invoke any registered pre-processors, for
compatibility
* `sparql-put`: store a copy of the new graph in the SPARQL store
* `deprecated:postprocess`: invoke any registered post-processors, for
compatiblity

The workflow can be altered by specifying a `workflow=...` value in the
`[twine]` configuration section:—

	[twine]
	;; A simple workflow which performs no processing and simply PUTs RDF
	;; graphs to a SPARQL server
	workflow=sparql-put

	;; A more complex workflow which passes graphs through a series of
	;; loaded processors that manipulate the data, before storing the graph
	;; via SPARQL PUT and finally invoking an additional indexing processor.
	workflow=myplugin-rearrange,anotherplugin-process,sparql-put,elasticsearch-indexer

Note that the actual graph processors that are available depends upon the
modules that you have loaded, and the above names are examples only.

### Clustering

Twine now has the ability to operate as part of a
[libcluster](https://github.com/bbcarchdev/libcluster)-based cluster. While this
does not meaningfully affect the operation of Twine itself, the cluster and node
status information is made available to plug-ins implementing message queues and
graph processors. For example, a message queue implementation might use the node
details to filter inbound messages from the queue in to balance load across the
cluster.

Cluster configuration is specified in the `[twine]` configuration section. If
absent, Twine will configure itself to be part of a single-node (i.e.,
standalone) cluster.

The default values are as follows:—

	[twine]
	cluster-name=twine
	cluster-verbose=no
	node-index=0
	cluster-size=1
	environment=production
	; registry=<registry URI>
	; node-id=<some unique identifier>

To use a registry service, remove the `node-index` and `cluster-size` options
and add a `registry` URI instead:

	[twine]
	cluster-name=twine
	cluster-verbose=no
	environment=live
	registry=http://registry:2323/


### API changes

The `libtwine` API has been reorganised with the aim of making it work more
consistently and supporting new feature enhancements. Twine will warn when
a plug-in is loaded which uses the older APIs and binary compatibility will
be maintained for the forseeable future. Source compatiblity is currently
being preserved (emitting compiler warnings where possible), but a future
release will require the explicit definition of a macro in order to continue
to make use of the deprecated APIs. Eventually the deprecated API prototypes
will be removed from the `libtwine.h` header altogether.

## Contributing

To contribute to Twine, fork this repository and commit your changes to the
`develop` branch. For larger changes, you should create a feature branch with
a meaningful name, for example one derived from the [issue
number](https://github.com/bbcarchdev/twine/issues/).

Once you are satisfied with your contribution, open a pull request and describe
the changes you’ve made.

## License

Twine is licensed under the terms of the [Apache License, Version
2.0](http://www.apache.org/licenses/LICENSE-2.0)

Copyright © 2014-2017 BBC.

[license]: https://img.shields.io/badge/license-Apache%202.0-blue.svg
