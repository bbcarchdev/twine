require 'net/http'
require 'openssl'
require 'rdf'
require 'rdf/ntriples'

def count(uri, offset=0, limit=100)
        graph = RDF::Graph.load("#{uri}?offset=#{offset}&limit=#{limit}")
        objs = [
                RDF::URI.new("http://purl.org/vocab/frbr/core#Work"),
                RDF::URI.new("http://xmlns.com/foaf/0.1/Person"),
                RDF::URI.new("http://shakespeare.acropolis.org.uk/ontologies/work#Play"),
                RDF::URI.new("http://shakespeare.acropolis.org.uk/ontologies/work#Sonnet"),
                RDF::URI.new("http://shakespeare.acropolis.org.uk/ontologies/work#Poem"),
        ]

        # Count entities on this page
        n = 0
        objs.each do |obj|
                graph.query([nil, RDF.type, obj]) do |st|
                        n += 1
                end
        end

        # Process all subsequent pages
        graph.query([nil, RDF::URI.new("http://www.w3.org/1999/xhtml/vocab#next"), nil]) do |st|
                n += count(uri, offset + limit)
        end

        return n
end

def uri(c_uri)
        uri = URI("http://quilt/")
        params = { :uri => c_uri }
        uri.query = URI.encode_www_form(params)
        response = Net::HTTP.get_response(uri)
        expect(response).to be_a(Net::HTTPSeeOther)

        return response['location'].chomp('#id')[1..-1]
end

def results(uri, params)
        # Define the query and execute it
        query = URI.encode_www_form(params)
        graph = RDF::Graph.load("#{uri}?#{query}")
        
        # Look for the English translation of this query and print it
        title_query = RDF::Query.new(
        RDF::URI.new("http://acropolis.localhost/?#{query}") => {
                RDF.type => RDF::URI.new("http://rdfs.org/ns/void#Dataset"),
                RDF::URI.new("http://www.w3.org/2000/01/rdf-schema#label") => :label
        })
        title_query.execute(graph).each do |entry|
                puts entry.label
        end
        
        # Process the results of the query
        results = Array.new
        slots_query = RDF::Query.new(
        :entry => {
                RDF.type => RDF::URI.new("http://purl.org/ontology/olo/core#Slot"),
                RDF::URI.new("http://purl.org/ontology/olo/core#index") => :index,
                RDF::URI.new("http://purl.org/ontology/olo/core#item") => :item
        })
        slots_query.execute(graph).each do |entry|
                results.insert(entry.index, entry.item)
        end
        
        # Remove any eventual nil value before and return the array
        return results.compact
end