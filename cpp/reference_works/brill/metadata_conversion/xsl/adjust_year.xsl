<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
                xmlns:marc="http://www.loc.gov/MARC21/slim">
<xsl:output omit-xml-declaration="yes" indent="no"/>
    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773']/marc:subfield[@code='g']/text()" priority="3">
        <xsl:variable name="year" select="ancestor::marc:datafield/preceding-sibling::marc:datafield[@tag='264']/marc:subfield[@code='c']"/>
        <xsl:value-of select="$year"/>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='936']/marc:subfield[@code='j']/text()">
        <xsl:variable name="year" select="ancestor::marc:datafield/preceding-sibling::marc:datafield[@tag='264']/marc:subfield[@code='c']"/>
        <xsl:value-of select="$year"/>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773']">
        <!-- <xsl:copy-of select="."/> -->
        <xsl:copy>
            <xsl:apply-templates select="node()|@*"/>
        </xsl:copy>
        <xsl:variable name="year" select="preceding-sibling::marc:datafield[@tag='264']/marc:subfield[@code='c']"/>
        <xsl:element name="datafield" namespace="http://www.loc.gov/MARC21/slim">
            <xsl:attribute name="tag">773</xsl:attribute>
            <xsl:attribute name="ind1">1</xsl:attribute>
            <xsl:attribute name="ind2">8</xsl:attribute>
            <xsl:element name="subfield" namespace="http://www.loc.gov/MARC21/slim">
                 <xsl:attribute name="code">g</xsl:attribute>
                 <xsl:text>year:</xsl:text><xsl:value-of select="$year"/>
            </xsl:element>
        </xsl:element>
    </xsl:template>
</xsl:stylesheet>
