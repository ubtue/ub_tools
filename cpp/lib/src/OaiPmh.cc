/** \file    OaiPmh.cc
 *  \brief   Definition of classes that support OAI-PMH servers and clients.
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *
 *  This file is part of the libiViaOaiPmh package.
 *
 *  The libiViaOaiPmh package is free software. You can redistribute
 *  it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  libiViaOaiPmh is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with libiViaOaiPmh; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 */

#include "OaiPmh.h"
#include <stdexcept>
#include "HtmlUtil.h"
#include "IniFile.h"
#include "StringUtil.h"


namespace OaiPmh {


// StringToHarvestMode --  Convert a string into a HarvestMode value.
//
HarvestMode StringToHarvestMode(const std::string &harvest_mode_str) {
    if (StringUtil::ToUpper(harvest_mode_str) == "FULL")
        return FULL;
    else if (StringUtil::ToUpper(harvest_mode_str) == "INCREMENTAL")
        return INCREMENTAL;

    // Error:
    throw std::runtime_error("in StringToHarvestMode (OaiPmh.cc):  unknown harvest mode: " + harvest_mode_str);
}


// Field::Field -- Construct an unqulaified OAI-PMH metadata element.
//
Field::Field(const std::string &field, const std::string &value, const std::string &attribute)
    : field_(field), value_(value), attribute_(attribute)
{
    if (not HtmlUtil::IsHtmlEscaped(value))
        throw std::runtime_error("in OaiPmh::Field constructor: metadata field value is not HTML escaped: field \""
                                 + field + "\", attribute \"" + attribute + "\","" value \"" + value + "\".");
}


// MetadataFormat -- Constructor for MetadataFormat.
//
MetadataFormat::MetadataFormat(const std::string &name, const IniFile &ini_file)
    : name_(name)
{
    // Sanity check:
    if (not ini_file.sectionIsDefined(name))
        throw std::runtime_error("metadata format '" + name + "' is not defined in configuration file '"
                                 + ini_file.getFilename() + "'.");

    // Get the XML "container" element that will wrap the metadata record:
    container_ = ini_file.getString(name, "container", "");

    // Read the set of namespaces and schema locations:
    IniFile::SectionContents section_contents = ini_file.getSection(name);
    for (IniFile::SectionContents::const_iterator name_value_pair(section_contents.begin());
         name_value_pair != section_contents.end();
         ++name_value_pair)
        if (std::strncmp("xmlns", name_value_pair->first.c_str(), 5) == 0
            or std::strncmp("xsi", name_value_pair->first.c_str(), 3) == 0
            or std::strcmp("schemaVersion", name_value_pair->first.c_str()) == 0)
            namespaces_and_schema_locations_.push_back(name_value_pair->first + "=\"" + name_value_pair->second
                                                       + "\"");

    // Grab the list of fields from the configuration file:
    std::string fields = ini_file.getString(name, "fields");
    std::list<std::string> field_section_list;
    StringUtil::SplitThenTrim(fields, ",", " \t\n\r", &field_section_list);

    // Iterate of the field sections reading the map:
    for (std::list<std::string>::const_iterator field_section(field_section_list.begin());
         field_section != field_section_list.end(); ++field_section)
    {
        const std::string field_name   (ini_file.getString(*field_section, "field_name", *field_section));
        const std::string xml_element  (ini_file.getString(*field_section, "xml_element", *field_section));
        const std::string xml_attribute(ini_file.getString(*field_section, "xml_attribute", ""));

        xml_element_map_.insert(std::make_pair(field_name, xml_element));
        xml_attribute_map_.insert(std::make_pair(field_name, xml_attribute));

    }
}


} // namespace OaiPmh
