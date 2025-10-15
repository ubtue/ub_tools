<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:sru="http://www.loc.gov/zing/srw/"
  xmlns:marc="http://www.loc.gov/MARC21/slim"
  exclude-result-prefixes="sru marc">

  <xsl:output method="xml" indent="yes" encoding="UTF-8"/>

  <!-- Root: create collection with default MARC namespace -->
  <xsl:template match="/">
    <collection xmlns="http://www.loc.gov/MARC21/slim">
      <xsl:apply-templates select="//sru:record/sru:recordData/marc:record"/>
    </collection>
  </xsl:template>

  <!-- Rebuild all marc:* elements with local-name, MARC namespace, and no prefix -->
  <xsl:template match="marc:*">
    <xsl:element name="{local-name()}" namespace="http://www.loc.gov/MARC21/slim">
      <xsl:apply-templates select="@* | node()"/>
    </xsl:element>
  </xsl:template>

  <!-- Copy all attributes, preserving their namespace if any -->
  <xsl:template match="@*">
    <xsl:attribute name="{local-name()}" namespace="{namespace-uri()}">
      <xsl:value-of select="."/>
    </xsl:attribute>
  </xsl:template>

  <!-- Copy text nodes fully -->
  <xsl:template match="text()" priority="55555">
    <xsl:value-of select="."/>
  </xsl:template>

  <!-- Copy comments and processing instructions -->
  <xsl:template match="comment() | processing-instruction()">
    <xsl:copy/>
  </xsl:template>

  <!-- Ignore all other elements and nodes -->
  <xsl:template match="node()"/>
</xsl:stylesheet>

