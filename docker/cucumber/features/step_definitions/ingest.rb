
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
    #puts "#{a}"

    # Assert if the response was OK
    expect(response).to be_a(Net::HTTPOK)
end

When(/^I update all the data currently ingested$/) do
        # GET the remote control to update the ingested data
        http = Net::HTTP.new('twine', 8000)
        http.read_timeout = 300

        request = Net::HTTP::Get.new("/update")
        response = http.request(request)

        # Assert if the response was OK
        expect(response).to be_a(Net::HTTPOK)
end

When(/^I count the amount of relevant entities that are ingested$/) do
        @entities = count("http://quilt/everything.nt")
        puts "(#{@entities} entities found)"
end

When(/^A collection exists for "([^"]*)"$/) do |c_uri|
        uri = URI("http://quilt/")
        params = { :uri => c_uri }
        uri.query = URI.encode_www_form(params)
        response = Net::HTTP.get_response(uri)

        expect(response).to be_a(Net::HTTPSeeOther)
        @collection = response['location'].chomp('#id')
end

Then(/^The number of relevant entities in the collection should be the same$/) do
        n = count("http://quilt#{@collection}.nt")

        expect(n).to eq(@entities)
end
