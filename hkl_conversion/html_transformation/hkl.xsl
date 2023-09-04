<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0"
                xmlns:xhtml="http://www.w3.org/1999/xhtml"
                xmlns:xs="http://www.w3.org/2001/XMLSchema"
                xmlns:mf="http://www.**.com"
                exclude-result-prefixes="xs xsl mf xhtml">
<xsl:output omit-xml-declaration="yes" indent="no"/>

    <xsl:function name="mf:get-top-offset-pixels">
        <xsl:param name="items" as="item()*"/>
        <xsl:variable name="styles" select="tokenize($items/@style, ';')"/>
        <xsl:variable name="top_prefix" select="'top:'"/>
        <xsl:for-each select="$styles">
            <xsl:variable name="style" select="normalize-space()"/>
            <xsl:if test="starts-with(., $top_prefix)">
                <!-- Strip prefix and 'px' at the end -->
                <xsl:variable name="top_prefix_len" select="string-length($top_prefix)"/>
                <xsl:sequence select="substring(., $top_prefix_len + 1, string-length(.) - $top_prefix_len - 2)"/>
            </xsl:if>
        </xsl:for-each>
    </xsl:function>

    <xsl:function name="mf:has-new-item-distance" as="xs:boolean">
        <xsl:param name="paragraph1" as="item()*"/>
        <xsl:param name="paragraph2" as="item()*"/>
        <xsl:variable name="paragraph1_top_offset" select="number(mf:get-top-offset-pixels($paragraph1))"/>
        <xsl:variable name="paragraph2_top_offset" select="number(mf:get-top-offset-pixels($paragraph2))"/>
        <xsl:variable name="test1" select="number($paragraph1_top_offset)"/>
        <xsl:variable name="test2" select="number($paragraph2_top_offset)"/>
        <xsl:value-of select="abs(number($paragraph1_top_offset) - number($paragraph2_top_offset)) >= 25"/>
    </xsl:function>

    <xsl:function name="mf:is-headline-paragraph" as="xs:boolean">
        <xsl:param name="items" as="item()*"/>
        <xsl:sequence select="number(mf:get-top-offset-pixels($items)) &lt; number('50')"/>
    </xsl:function>

    <xsl:function name="mf:get-left-offset-pixels">
        <xsl:param name="items" as="item()*"/>
        <xsl:variable name="styles" select="tokenize($items/@style, ';')"/>
        <xsl:variable name="left_prefix" select="'left:'"/>
        <xsl:for-each select="$styles">
            <xsl:variable name="style" select="normalize-space()"/>
            <xsl:if test="starts-with(., $left_prefix)">
                <!-- Strip prefix and 'px' at the end -->
                <xsl:variable name="left_prefix_len" select="string-length($left_prefix)"/>
                <xsl:sequence select="substring(., $left_prefix_len + 1, string-length(.) - $left_prefix_len - 2)"/>
            </xsl:if>
        </xsl:for-each>
    </xsl:function>

    <xsl:function name="mf:is-author-heading" as="xs:boolean">
        <xsl:param name="items" as="item()*"/>
        <xsl:sequence select="number(mf:get-left-offset-pixels($items)) > 150"/>
    </xsl:function>


    <xsl:template match="@* | node()">
      <xsl:copy>
        <xsl:apply-templates select="@* | node()"/>
      </xsl:copy>
    </xsl:template>
    <xsl:template match="xhtml:p">
         <xsl:choose>
             <xsl:when test="mf:is-headline-paragraph(.)">
                 <!-- skip these elements -->
             </xsl:when>
             <xsl:when test="mf:is-author-heading(.)">
                     AUTHOR HEADING <xsl:value-of select="."/>
             </xsl:when>
             <xsl:when test="preceding-sibling::*[1] and  mf:has-new-item-distance(., preceding-sibling::*[1])">
                 NEUER TITEL
                 <xsl:copy-of select="."/>
             </xsl:when>
             <xsl:otherwise>
                 <xsl:value-of select="."/>
             </xsl:otherwise>
         </xsl:choose>
    </xsl:template>
</xsl:stylesheet>
