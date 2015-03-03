Spindle is a co-reference aggregation engine which works as a
post-processing hook for Twine.

When a graph is updated by a Twine processor, Spindle examines the contents
of the graph and looks for owl:sameAs co-reference assertions. Where some are
found, it generates "proxy" entities within a named graph that connect
together all of the co-references for a given source entity, and as more
co-references are discovered, they're also added to the proxy entity.

For example, if graphs are processed which assert that <A> owl:sameAs <B>,
<B> owl:sameAs <C> and <C> owl:sameAs <D>, the end result is the following:

<http://spindle.example.com/> {

  <http://spindle.example.com/abc123#id>
    owl:sameAs <A>, <B>, <C>, <D> .

}

By collecting together the co-references in this fashion, it becomes
possible to deal with a unified view (stored as quads internally) of
a given entity described by multiple disparate graphs.

In other words, Spindle constructs a topic-oriented index of all of the
entities processed by Twine.

The process in detail:

1. During initialisation, Spindle loads a rule-base describing predicates
   and classes which it'll pay attention to from a Turtle-serialised RDF
   file. The original source is in rulebase.ttl in this directory.

2. Prior to graph store, a preprocessor will strip out any triples whose
   predicates are not listed in rulebase.ttl

3. Twine will replace the existing graph with the new graph from the source
   data.

4. The Spindle post-processor is invoked, which initially evaluates the
   subjects in the new graph. Each subject is given a local identifier,
   which may be an identifier it already has (if this is an update), or
   the identifier of something which asserts that it is owl:sameAs our
   new subject, or if all else fails, a URI incorporating a freshly-generated
   UUID.

5. For each subject, an assertion is stored in the root graph that
   <subject-uri> owl:sameAs </:local-uuid#id>, and </local-uuid#id> is added
   to the list of updated local entries (proxies).

6. For each updated proxy in the list, a caching operation is triggered. The
   cache build fetches all of the source data relating to all of the
   (remote) subjects related to the local proxy URI.

7. Next, any entities which refer to any of the remote subjects of this proxy
   are added to the cache-update list (this is so that internal links can
   be generated and updated when proxies are created). 

8. Following the rulebase, classes and properties are copied from the source
   data into the new proxy. The behaviour of property copies can vary: for
   resources, the rulebase can specify whether the property should only be
   copied if there's a local equivalent of the target resource (i.e., it is
   only expressed when it's possible to create an external link); for literals,
   a scoring mechanism is applied which is able to make a determination
   based upon the predicate used in the source data, the languages, and
   datatypes of the literal triples. Properties marked as being indexed are
   added to both the proxy and the root graphs.

9. Next, triples are added to the proxy graph which state that the source
   graph URIs are foaf:Documents, and that the source subjects are
   wdrs:describedBy the graph URIs (creating a link chain between the local
   proxy and the original source graph).

10. The proxy is stored in a graph whose name is </local-uuid> (distinct
    from the proxy's URI itself, which is </local-uuid#id>). This means that
    a front-end server can retrieve the proxy data by retrieving all of
    the information in the proxy's own graph.

11. If caching to an S3 bucket is enabled, information about specific
    resources referred to by the proxy (either via foaf:page or
    mrss:player) is fetched and stored in an 'extra' model.

12. Finally, the proxy graph, the source data, and the extra data are all
    serialised as N-Quads and written to the S3 bucket.
