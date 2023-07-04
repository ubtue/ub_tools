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
              <xsl:text>1</xsl:text>
           </xsl:attribute>
           <xsl:attribute name="ind2">
              <xsl:text> </xsl:text>
           </xsl:attribute>
           <xsl:apply-templates select="node()"/>
        </xsl:copy>
    </xsl:template>

    <!-- Move URL to 856u -->
    <xsl:template match="datafield[@tag='534']">
        <xsl:copy>
           <xsl:attribute name="tag">
               <xsl:value-of select="856"/>
           </xsl:attribute>
           <xsl:attribute name="ind1">
               <xsl:text>4</xsl:text>
           </xsl:attribute>
           <xsl:attribute name="ind2">
               <xsl:text>0</xsl:text>
           </xsl:attribute>
           <subfield code="u">
               <xsl:copy-of select="./subfield[@code='t']/text()"/>
           </subfield>
           <subfield code="z">ZZ</subfield>
        </xsl:copy>
    </xsl:template>

    <!-- Move date to 264c year -->
    <xsl:template match="datafield[@tag='260' and subfield[@code='c']]">
        <xsl:copy>
           <xsl:attribute name="tag">
                <xsl:value-of select="264"/>
           </xsl:attribute>
           <xsl:attribute name="ind1">
              <xsl:text> </xsl:text>
           </xsl:attribute>
           <xsl:attribute name="ind2">
              <xsl:text>1</xsl:text>
           </xsl:attribute>
           <subfield code="c">
               <xsl:copy-of select="substring(normalize-space(./subfield[@code='c']/text()),0,5)"/>
           </subfield>
       </xsl:copy>
    </xsl:template>

    <!-- Remove redundant 260 Brill content -->
    <xsl:template match="datafield[@tag='260' and subfield[@code='b']]" priority="2"/>

    <!-- Remove superflous 655 Brill content field -->
    <xsl:template match="datafield[@tag='655' and subfield[@code='a'][.='text'] and subfield[@code='2'][.='dct']]"/>
    <xsl:template match="datafield[@tag='024']">
        <xsl:copy-of select="."/>
        <xsl:element name="datafield" namespace="http://www.loc.gov/MARC21/slim">
        <xsl:attribute name="tag">856</xsl:attribute>
        <xsl:attribute name="ind1">4</xsl:attribute>
        <xsl:attribute name="ind2">0</xsl:attribute>
            <xsl:element name="subfield" namespace="http://www.loc.gov/MARC21/slim">
              <xsl:attribute name="code">u</xsl:attribute>
              <xsl:text>https://doi.org/</xsl:text><xsl:value-of select="./subfield[@code='a']/text()"/>
            </xsl:element>
            <xsl:element name="subfield" namespace="http://www.loc.gov/MARC21/slim">
              <xsl:attribute name="code">z</xsl:attribute>
              <xsl:text>ZZ</xsl:text>
            </xsl:element>
        </xsl:element>
    </xsl:template>


    <!-- Remove superfluous whitespace -->
    <xsl:template match="datafield[not(@tag='007' or @tag='008')]//text()">
        <xsl:value-of select="normalize-space()"/>
    </xsl:template>

</xsl:stylesheet>

