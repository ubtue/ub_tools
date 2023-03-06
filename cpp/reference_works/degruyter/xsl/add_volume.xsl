<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0"
                xmlns:marc="http://www.loc.gov/MARC21/slim">
<xsl:output omit-xml-declaration="yes" indent="no"/>
<xsl:variable name="ahead_of_print" select="'Ahead of Print'"/>
    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773' and @ind1='0' and @ind2='8']/marc:subfield[@code='g']/text()">
        <xsl:variable name="year" select="."/>
        <xsl:variable name="volume" select="ancestor::marc:datafield/following-sibling::marc:datafield[@tag='VOL']/marc:subfield[@code='a']"/>
        <xsl:choose>
            <xsl:when test="not($volume = $ahead_of_print)">
               <xsl:value-of select="$volume"/><xsl:text>(</xsl:text>
               <xsl:value-of select="$year"/><xsl:text>)</xsl:text>
             </xsl:when>
             <xsl:otherwise>
                <xsl:value-of select="$year"/>
             </xsl:otherwise>
        </xsl:choose>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773' and @ind1='1' and @ind2='8']/*">
        <xsl:variable name="volume" select="ancestor::marc:datafield/following-sibling::marc:datafield[@tag='VOL']/marc:subfield[@code='a']"/>
        <xsl:if test="not($volume = $ahead_of_print)">
            <xsl:element name="subfield" namespace="http://www.loc.gov/MARC21/slim">
                <xsl:attribute name="code">g</xsl:attribute>
                <xsl:text>volume:</xsl:text><xsl:value-of select="$volume"/>
            </xsl:element>
        </xsl:if>
        <xsl:copy-of select="."/>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='936' and @ind1='u' and @ind2='w']/*">
        <xsl:variable name="volume" select="ancestor::marc:datafield/following-sibling::marc:datafield[@tag='VOL']/marc:subfield[@code='a']"/>
        <xsl:if test="not($volume = $ahead_of_print)">
            <xsl:element name="subfield" namespace="http://www.loc.gov/MARC21/slim">
                <xsl:attribute name="code">d</xsl:attribute>
                   <xsl:value-of select="$volume"/>
            </xsl:element>
        </xsl:if>
        <xsl:copy-of select="."/>
    </xsl:template>
</xsl:stylesheet>
