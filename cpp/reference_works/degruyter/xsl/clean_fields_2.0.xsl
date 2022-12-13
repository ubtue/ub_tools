<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0"
                xmlns:marc="http://www.loc.gov/MARC21/slim">
<xsl:strip-space elements="*"/>
<xsl:output omit-xml-declaration="yes" indent="yes"/>
    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>

    <!-- For 264 we are only interested in the year, so skip other subfields -->
    <xsl:template match="marc:datafield[@tag='264' and @ind2='1']/marc:subfield[@code!='c']"/>
    <xsl:template match="marc:datafield[@tag='264' and @ind2='1']/marc:subfield[@code='c']" priority="3">
        <xsl:copy>
           <xsl:copy-of select="@*"/>
           <xsl:variable name="year" select="./text()"/>
           <xsl:value-of select="replace(., '[\[]?(\d+)[\]]?', '$1')"/>
       </xsl:copy>
    </xsl:template>
    <!-- Remove second 264 field -->
    <xsl:template match="marc:datafield[@tag='264' and @ind2='4']" priority="0"/>

    <!-- Remove 245c -->
    <xsl:template match="marc:datafield[@tag='245']/marc:subfield[@code='c']"/>

    <!-- Fix authors, i.e. remove garbish at the end -->
    <xsl:template match="marc:datafield[@tag='100' or @tag='700']/marc:subfield[@code='a']/text()">
         <xsl:value-of select="replace(., '(.*?)[,.\s]+$' , '$1')"/>
    </xsl:template>

    <!-- Fix title, i.e remove garbish at the end -->
    <xsl:template match="marc:datafield[@tag='245']/marc:subfield[@code='a']/text()">
        <xsl:value-of select="replace(., '(.*?)[,.\s/]+$', '$1')"/>
    </xsl:template>
</xsl:stylesheet>
