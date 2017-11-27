# Twine
A workflow engine for processing [RDF](https://www.w3.org/RDF/) in customisable ways.

[![Apache 2.0 licensed][license]](#license)

## Table of contents

* [What is it?](#what-is-it)
* [History](#history)
* [Requirements](#requirements)
* [Changes as of Twine 7.x](#changes)
* [Building from source](#building-from-source)
* [Source tree structure](#source-tree-structure)
* [Plug-ins](#plug-ins)
* [Contributing](#contributing)
* [License](#license)

## What is it?

Twine is a server that receives messages from a queue, and then performs one or
more processing steps on it. All of the processing steps operate on sets of
RDF graphs. The exact processing steps depend upon a _workflow_ specified
in Twine's configuration.

Plug-in modules provide message _handlers_ (code which understands different
message formats and can convert them to RDF for processing), as well as workflow
processors which operate on the RDF graphs.

## History

Twine was originally written to receive data in multiple formats, and with
a small amount of format-specific code (or an XSLT stylesheet), push an RDF
representation of that data into a graph store via SPARQL. It can still be
used for this purpose via the `sparql-put` workflow processor.

## Requirements

Twine requires the following common libraries which are available either
out-of-the-box, or can be installed using native package management, on most
Unix-like operating systems:

* POSIX threads and libdl (generally shipped as part of the core C libraries on most Unix-like systems)
* [Apple CommonCrypto](https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/Common%20Crypto.3cc.html#//apple_ref/doc/man/3cc/CommonCrypto) or [OpenSSL](https://www.openssl.org)
* [cURL](https://curl.haxx.se)
* [libxml2](http://www.xmlsoft.org) and [libxslt](http://xmlsoft.org/XSLT/)
* [librdf](http://librdf.org)

Note that in order to process source data in TriG or N-Quads formats correctly,
you must use an up-to-date version of librdf. If you have an older version
installed on your system, Twine will compile correctly, but will not be able to 
locate any named graphs in the parsed RDF quads when attempting to import them.

Twine also requires the following libraries which are available from
the [BBC Archive Development Github account](https://github.com/bbcarchdev):

* [libcluster](https://github.com/bbcarchdev/libcluster)
* [libmq](https://github.com/bbcarchdev/libmq)
* [libawsclient](https://github.com/bbcarchdev/libawsclient)
* [liburi](https://github.com/bbcarchdev/liburi)
* [libsparqlclient](https://github.com/bbcarchdev/libsparqlclient)

Finally, building Twine requires working GNU autotools (`make`, `autoconf`,
`automake`, `libtool`, and so on) as well as a C compiler and your operating
system’s usual developer packages.

## Changes

Significant changes as of Twine 7.x:—

### Configuration file structure

The configuration file structure has been simplified, although
older settings will be used (emitting a warning) if they are present.

The majority of Twine’s configuration is now in the `[twine]` section, whose
options apply to both the `twine` command-line utility, the `twine-writerd`
daemon and the `twine-inject` tool. Individual options can be overridden by
adding them to specific `[twine-cli]`, `[writerd]` or `[inject]` sections.
Plug-ins will continue to use their own 

See the [configuration file example](https://github.com/bbcarchdev/twine/blob/develop/conf/twine.conf.in) for further details.

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
* `deprecated:preprocess`: invoke any registered pre-processors, for compatibility
* `sparql-put`: store a copy of the new graph in the SPARQL store
* `deprecated:postprocess`: invoke any registered post-processors, for compatiblity

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

Twine now has the ability to operate as part of a [libcluster](https://github.com/bbcarchdev/libcluster)
cluster. While this does not meaningfully affect the operation of Twine itself,
the cluster and node status information is made available to plug-ins
implementing message queues and graph processors. For example, a message
queue implementation might use the node details to filter inbound messages
from the queue in to balance load across the cluster.

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

* `libsupport` contains support for handling configuration files and logging
and is used by (and built into) `libtwine`
* `libutils` contains some supporting code which will over time be merged into
`libtwine`
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

## Plug-ins

Plug-ins can perform various different functions within Twine. In principle,
they can perform any action one might reasonably wish to on some RDF data.

Typically, they will:

* Deal with indirection: where messages don't contain actual source data, but
the location where it can be found (e.g., the `s3` plug-in).
* Handle different input formats, parsing and translating source data into
RDF so that it can be further processed or stored (e.g., the `xslt` plug-in).
* Perform transformation on RDF data (such as stripping triples out, or
generating new ones) - [Spindle](https://github.com/bbcarchdev/spindle/tree/develop/twine) is an example of a plug-in which does this.
* Storing the resultant RDF data somewhere, via SPARQL Update or some other
means (e.g., the built-in `sparql-put` plug-in).

### rdf

The `rdf` input plug-in parses messages (which may be files on disk or piped via
standard input if the standalone `twine` utility is used) which contain TriG-
or N-Quads-formatted RDF. The resulting graphs are then available for
transformation by other modules, or simply for storing (for example via the
built-in `sparql-put` processor).

### s3

The `s3` input plug-in handles messages which have a type of
`application/x-s3-url` and which are expected to contain a series of one or
more URLs (one per line) in the form `s3://BUCKETNAME/OBJECTNAME`. These URLs
are fetched in turn, using the S3 endpoint and credentials specified in the
Twine configuration file, and each is passed as-is back to Twine for
processing as though their contents had been sent directly.

### xslt

The `xslt` input plug-in is a configurable transformation processor which can
translate XML documents into a form such that they can be parsed as RDF/XML
and used for onward processing. The plug-in registers itself as being
able to handle MIME types specified in the Twine configuration file, and
applies the corresponding XSL stylesheet to the source document in order to
transform it into RDF/XML before parsing.

An example stylesheet and source XML document is provided as part of the
plug-in.

### geonames

The `geonames` input plug-in reads data in the GeoNames RDF dump format, which
consists of a repeating sequence of the following, each separated by newlines:

* Geonames URL (e.g., `http://sws.geonames.org/3/`)
* RDF/XML document for this graph (with any newlines stripped out)

The plug-in simply reads the Geonames URL and transforms it into a graph URI
(by appending `about.rdf`), then reads the RDF/XML document into an RDF model
as part of that named graph. The resulting model is pushed into Twine for
onward processing or storage.

For example, with the `rdf` and `geonames` plug-ins loaded, a GeoNames can be
converted to N-Quads with:

```
$ twine -D twine:workflow=dump-nquads -t text/x-geonames-dump all-geonames-rdf.txt > geonames.nq
```

## Contributing

To contribute to Twine, fork this repository and commit your changes to the
`develop` branch. For larger changes, you should create a feature branch with
a meaningful name, for example one derived from the [issue number](https://github.com/bbcarchdev/twine/issues/).

Once you are satisfied with your contribution, open a pull request and describe
the changes you’ve made.

## License

Twine is licensed under the terms of the [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)

Copyright © 2014-2016 BBC.

[license]: https://img.shields.io/badge/license-Apache%202.0-blue.svg
