#encoding: utf-8
Feature: Ingesting nquad files into Twine/Spindle

Scenario Outline: Ingesting nquads
	When "<file>" is ingested into Twine
	Then "<proxies>" proxies should exist in the database

	Examples: Test files
		| file | proxies |
		| test.nq | 4 |
