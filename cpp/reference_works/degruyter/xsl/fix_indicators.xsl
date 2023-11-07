<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0"
                xmlns:marc="http://www.loc.gov/MARC21/slim">
<xsl:output omit-xml-declaration="yes" indent="no"/>
    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773']/@ind1">
        <xsl:attribute name="{name()}">
            <xsl:choose>
               <xsl:when test="current() = ' '">0</xsl:when>
               <xsl:otherwise><xsl:value-of select="."/></xsl:otherwise>
            </xsl:choose>
        </xsl:attribute>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='100']/@ind1">
        <xsl:attribute name="{name()}">1</xsl:attribute>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773']/@ind2">
        <xsl:attribute name="{name()}">8</xsl:attribute>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='936']/@ind1">
         <xsl:attribute name="{name()}">u</xsl:attribute>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='936']/@ind2">
         <xsl:attribute name="{name()}">w</xsl:attribute>
    </xsl:template>


</xsl:stylesheet>
