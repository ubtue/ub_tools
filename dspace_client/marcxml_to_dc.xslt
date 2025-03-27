<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.1" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"  xmlns:marc="http://www.loc.gov/MARC21/slim" xmlns:zs="http://www.loc.gov/zing/srw/">
    <xsl:output method="text" encoding="UTF-8" omit-xml-declaration="yes" indent="yes" />
    <xsl:template match="/">
            <xsl:apply-templates/>
    </xsl:template>
    <xsl:template match="text()"/>

    <xsl:template match="marc:record">
        <xsl:text>{</xsl:text>
        <xsl:text>"utue.artikel.ppn": "</xsl:text><xsl:value-of select='//marc:controlfield[@tag="001"]'/><xsl:text>",</xsl:text>
        <xsl:text>"dc.contributor.author": [</xsl:text>
        <xsl:for-each select="marc:datafield[@tag=100]|marc:datafield[@tag=110]|marc:datafield[@tag=111]|marc:datafield[@tag=700]|marc:datafield[@tag=710]|marc:datafield[@tag=711]|marc:datafield[@tag=720]">
            <xsl:text>{</xsl:text>
            <xsl:text>"name": "</xsl:text><xsl:value-of select='./marc:subfield[@code="a"]'/><xsl:text>",</xsl:text>
            <xsl:choose>
                <xsl:when test="starts-with(marc:subfield[@code='0'], '(DE-588)')">
                    <xsl:text>"gdn": "</xsl:text><xsl:value-of select="substring-after(marc:subfield[@code='0'], '(DE-588)')"/>
                    <xsl:text>",</xsl:text>
                </xsl:when>
            </xsl:choose>     
            <xsl:text>"role": "</xsl:text><xsl:value-of select='//marc:subfield[@code="4"]'/><xsl:text>"</xsl:text>
            <xsl:text>}</xsl:text>
            <xsl:if test="position() != last()">
                <xsl:text>, </xsl:text> <!-- Add a comma if not the last item -->
            </xsl:if>
        </xsl:for-each>
        <xsl:text>],</xsl:text>
        <xsl:text>"dc.date.issued": "</xsl:text><xsl:value-of select='//marc:datafield[@tag="260"]/marc:subfield[@code="c"]'/>"<xsl:text>,</xsl:text>
        <xsl:text>"dc.language.iso": </xsl:text>"<xsl:value-of select='//marc:datafield[@tag="041"]/marc:subfield[@code="a"]'/>"<xsl:text>,</xsl:text>
        <xsl:text>"dc.publisher": "</xsl:text><xsl:value-of select='//marc:datafield[@tag="260"]/marc:subfield[@code="b"]'/>"<xsl:text>,</xsl:text>
        <xsl:text>"dc.title": "</xsl:text><xsl:value-of select='//marc:datafield[@tag="245"]/marc:subfield[@code="a"]'/>"
        <xsl:text>}</xsl:text>
    </xsl:template>
</xsl:stylesheet>
