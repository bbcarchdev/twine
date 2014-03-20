<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0' xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#' xmlns:dct='http://purl.org/dc/terms/'>
  <xsl:template match='/node()'>
	<xsl:variable name='id'><xsl:copy-of select='/*/id' /></xsl:variable>
	<rdf:RDF>
	  <rdf:Description>
		<xsl:attribute name='rdf:about'>http://example.com/things/<xsl:value-of select='$id' />#id</xsl:attribute>
		<dct:title><xsl:copy-of select='/*/title/node()' /></dct:title>
		<dct:description><xsl:copy-of select='/*/description/node()' /></dct:description>
	  </rdf:Description>
	</rdf:RDF>
  </xsl:template>
</xsl:stylesheet>
