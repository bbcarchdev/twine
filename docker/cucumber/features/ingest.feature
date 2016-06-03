#encoding: utf-8
Feature: Ingesting nquad files into Twine/Spindle

Scenario: Ingesting Shakespeare collection sample
	When "all-works.nq" is ingested into Twine
	And I update all the data currently ingested
	And I count the amount of relevant entities that are ingested
	And A collection exists for "http://shakespeare.acropolis.org.uk/#id"
	Then The number of relevant entities in the collection should be the same
