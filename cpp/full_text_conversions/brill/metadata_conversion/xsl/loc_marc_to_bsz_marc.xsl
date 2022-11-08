<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:output omit-xml-declaration="yes" indent="no"/>
    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>
    <!-- Remove Brill author from 720-->
    <xsl:template match="datafield[@tag='720' and subfield[@code='a'][.='Brill']]" priority="2"/>

    <!-- Move Author to 100 and 700-->
    <xsl:template match="datafield[@tag='720']" priority="1">
        <xsl:variable name="author_tag">
            <xsl:choose>
                <xsl:when test="boolean(preceding-sibling::datafield[@tag='720'])">700</xsl:when>
                <xsl:otherwise>100</xsl:otherwise>
            </xsl:choose>
        </xsl:variable>
        <xsl:copy>
           <xsl:attribute name="tag">
              <xsl:value-of select="$author_tag"/>
           </xsl:attribute>
           <xsl:attribute name="ind1">
              <xsl:text> </xsl:text>
           </xsl:attribute>
           <xsl:attribute name="ind2">
              <xsl:text> </xsl:text>
           </xsl:attribute>
           <xsl:copy-of select="node()"/>
        </xsl:copy>
    </xsl:template>

    <!-- Move URL to 856 u -->
    <xsl:template match="datafield[@tag='534']">
        <xsl:copy>
           <xsl:attribute name="tag">
                <xsl:value-of select="856"/>
           </xsl:attribute>
           <xsl:attribute name="ind1">
              <xsl:text> </xsl:text>
           </xsl:attribute>
           <xsl:attribute name="ind2">
              <xsl:text> </xsl:text>
           </xsl:attribute>
           <subfield code="u">
               <xsl:copy-of select="./subfield[@code='t']/text()"/>
           </subfield>
           <subfield code="z">ZZ</subfield>
        </xsl:copy>
     </xsl:template>

</xsl:stylesheet>

