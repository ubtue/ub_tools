<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0" 
                xmlns:marc="http://www.loc.gov/MARC21/slim">
<xsl:strip-space elements="*"/>    
<xsl:output omit-xml-declaration="yes" indent="yes"/>
    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>

    <!-- Remove first 264 field with year in square brackets since we are only interested in the year --> 
    <xsl:template match="marc:datafield[@tag='264'][marc:subfield[@code='c'][starts-with(., '[')]]" priority="3"/>

    <!-- Remove 245c -->
    <xsl:template match="marc:datafield[@tag='245']/marc:subfield[@code='c']"/>

    <!-- Fix year scheme in second 264 field -->
    <xsl:template match="marc:datafield[@tag='264']/marc:subfield[@code='c']/text()">
        <xsl:variable name="year" select="."/>
        <xsl:value-of select="replace($year, '©', '')"/>
    </xsl:template>

    <!-- Fix indicators of the remaining 264 field -->    
    <xsl:template match="marc:datafield[@tag='264']/@ind1">
        <xsl:attribute name="ind1">
            <xsl:text> </xsl:text>
        </xsl:attribute>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='264']/@ind2">
         <xsl:attribute name="ind2" select="1"/>
     </xsl:template>

    <!-- Fix authors, i.e. remove garbish at the end -->
    <xsl:template match="marc:datafield[@tag='100' or @tag='700']/marc:subfield[@code='a']/text()">
         <xsl:value-of select="replace(., '(.*?)[,.\s]+$' , '$1')"/>
    </xsl:template>

    <!-- Fix title, i.e remove garbish at the end -->
    <xsl:template match="marc:datafield[@tag='245']/marc:subfield[@code='a']/text()">
        <xsl:value-of select="replace(., '(.*?)[,.\s/]+$', '$1')"/>
    </xsl:template>

    <!-- Move year to 936j and 773$g -->
    <xsl:template match="marc:datafield[@tag='936']/marc:subfield[@code='j']/text()">
        <xsl:value-of select="replace(../../../marc:datafield[@tag='264' and @ind2='4']/marc:subfield[@code='c']/text(), '©', '')"/>
    </xsl:template>
    <xsl:template match="marc:datafield[@tag='773']/marc:subfield[@code='g']/text()">
        <xsl:value-of select="replace(../../../marc:datafield[@tag='264' and @ind2='4']/marc:subfield[@code='c']/text(), '©', '')"/>
    </xsl:template>



</xsl:stylesheet>
