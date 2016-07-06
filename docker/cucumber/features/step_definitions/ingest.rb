#Before('@ingest')
Before do
        $dunit ||= false
        if $dunit == false
                step "\"shakespeare-sample.nq\" is ingested into Twine"
                $dunit = true
        end
end

# Internal step
When(/^"([^"]*)" is ingested into Twine$/) do |file|
        # POST the to the remote control the file to be ingested
        http = Net::HTTP.new('twine', 8000)
        http.read_timeout = 300

        request = Net::HTTP::Post.new("/ingest")
        request.add_field('Content-Type', 'text/x-nquads')
        request.body = IO.read(file)
        response = http.request(request)

        # Print the logs
        a = response.body()
        # puts "#{a}"

        # Assert if the response was OK
        expect(response).to be_a(Net::HTTPOK)
end

When(/^I count the amount of relevant entities that are ingested$/) do
        @entities = count("http://quilt/everything.nt")
        puts "(#{@entities} entities found)"
end

When(/^A proxy exists for "([^"]*)"$/) do |uri|
        @proxy = uri(uri)
end

Then(/^The number of relevant entities in the collection should be the same$/) do
        n = count("http://quilt/#{@proxy}.nt")
        expect(n).to eq(@entities)
end

When(/^The proxy is associated with "([^"]*)"'s proxy$/) do |uri|
        subj_proxy = uri(uri)
        graph = RDF::Graph.load("http://quilt/#{subj_proxy}")
        subj = RDF::URI.new("http://acropolis.localhost/#{subj_proxy}#id")
        pred = RDF::URI.new("http://purl.org/ontology/olo/core#slot")

        found = false
        graph.query([subj, pred, nil]) do |st|
                if st.object.end_with?(@proxy)
                        found = true
                end
        end

        expect(found).to eq(true)
end

When(/^The proxy is listed in the graph of "([^"]*)"'s proxy$/) do |uri|
        subj_proxy = uri(uri)
        graph = RDF::Graph.load("http://quilt/#{subj_proxy}")
        subj = RDF::URI.new("http://acropolis.localhost/#{@proxy}#id")

        found = false
        graph.query([subj, nil, nil]) do |st|
                found = true
        end

        expect(found).to eq(true)
end

When(/^I search for media for "([^"]*)"$/) do |audience|
        params = { "for" => audience }
        @entities = results("http://quilt/", params)
        puts "(#{@entities.count} results found)"
end

Then(/^The proxy is listed in the search results$/) do
  proxyURI = subj = RDF::URI.new("http://acropolis.localhost/#{@proxy}#id")
  expect(@entities).to include(proxyURI)
end
