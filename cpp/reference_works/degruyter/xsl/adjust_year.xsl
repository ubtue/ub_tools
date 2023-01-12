<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0" 
                xmlns:marc="http://www.loc.gov/MARC21/slim">
<xsl:output omit-xml-declaration="yes" indent="no"/>
    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773']/marc:subfield[@code='g']/text()">
        <xsl:variable name="year" select="ancestor::marc:datafield/preceding-sibling::marc:datafield[@tag='264']/marc:subfield[@code='c']"/>
        <xsl:value-of select="$year"/>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='936']/marc:subfield[@code='j']/text()">
        <xsl:variable name="year" select="ancestor::marc:datafield/preceding-sibling::marc:datafield[@tag='264']/marc:subfield[@code='c']"/>
        <xsl:value-of select="$year"/>
    </xsl:template>

</xsl:stylesheet>
