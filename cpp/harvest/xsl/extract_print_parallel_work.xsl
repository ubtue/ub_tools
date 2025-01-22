<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:marc="http://www.loc.gov/MARC21/slim"
  exclude-result-prefixes="marc">

  <!-- Output method is XML -->
  <xsl:output method="text" encoding="UTF-8"/>

  <!-- Match the root node -->
  <xsl:template match="/">
      <!-- Match each record -->
      <xsl:for-each select="marc:collection/marc:record">
        <!-- Check if the 776n element equals "Druck-Ausgabe" -->
        <xsl:for-each select="marc:datafield[@tag='776']">
          <xsl:if test="marc:subfield[@code='n'] = 'Druck-Ausgabe'">
              <!-- Extract the record id -->
              <xsl:value-of select="../marc:controlfield[@tag='001']"/>
              <xsl:text>:</xsl:text>
              <!-- Extract the PPN from 776w -->
              <xsl:value-of select="substring-after(marc:subfield[@code='w'], '(DE-627)')"/>
              <xsl:text>&#xa;</xsl:text>
          </xsl:if>
        </xsl:for-each>
      </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
