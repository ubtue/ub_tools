/** \file    OaiPmh.h
 *  \brief   Declaration of classes used to implement OaiPmh servers and clients.
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2003-2006 Project iVia.
 *  Copyright 2003-2006 The Regents of The University of California.
 *
 *  This file is part of the libiViaOaiPmh package.
 *
 *  The libiViaOaiPmh package is free software.  You can redistribute
 *  it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  libiViaOaiPmh is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaOaiPmh; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef OAI_PMH_H
#define OAI_PMH_H


#ifndef LIST
#       include <list>
#       define LIST
#endif
#ifndef MAP
#       include <map>
#       define MAP
#endif
#ifndef STRING
#       include <string>
#       define STRING
#endif
#ifndef INI_FILE_H
#       include <IniFile.h>
#endif


namespace OaiPmh {


/** \enum   HarvestMode
 *  \brief  Controls whether an OAI-PMH client performs a full harvest or an incremental harvest.
 *
 *  OAI-PMH allows clients us to import only those records
 *  that have changed since the last time we performed a harvest.
 */
enum HarvestMode {
	FULL,        //!< The progress file is ignored, and all records are harvested.
	INCREMENTAL  //!< Use progress file to import only the records that have changed since the last import.
};


/** \brief   Convert a string into a HarvestMode value.
 *  \param   harvest_mode_str  The string representation of the harvest mode.
 *  \return  The equivilent HarvestMode value.
 */
HarvestMode StringToHarvestMode(const std::string &harvest_mode_str);


/** \brief  Represents a generic metadata element as a field (or name), a value, and an optional type attribute.
 */
class Field {
	/** The metadata field name (e.g. 'subject'). */
	std::string field_name_;

	/** The OAI-PMH metadata value as a string (e.g. 'History -- United States'). */
	std::string value_;

	/** An (optional) attribute describing some feeature of the record, often its type. */
	std::string attribute_;
public:
	/** \brief  Construct an unqulaified OAI-PMH metadata element.
	 *  \param  field_name  The metadata field name.
	 *  \param  value       The metadata field value.
	 *  \param  attribute   An optional attribute describing the metadata field type.
	 *
	 *  The value must be HTML escaped, otherwise an exception
	 *  will be thrown.  The value is assumed to be ISO 8859.15 encoded.
	 */
	Field(const std::string &field, const std::string &value, const std::string &attribute = "");

	/** Destroy a metadata element. */
	virtual ~Field() { }

	/** Get the metadata field. */
	std::string getFieldName() const { return field_name_; }

	/** Get the metadata value. */
	std::string getValue() const { return value_; }

	/** Get the optional metadata attrbiute. */
	std::string getAttribute() const { return attribute_; }
private:
	Field(); // Intentionally unimplemented!
};


/** \brief  Represents an OAI-PMH identifier comprising an identifier string and a modification date.
 */
class Identifier {

	/** The OAI-PMH identifier of the record. */
	std::string identifier_;

	/** The date and time that the record was last modified. */
	std::string last_modification_timestamp_;

public:

	/** \brief  Contruct a metadata record.
	 *  \param  identifier                    The record identifier.
	 *  \param  last_modification_timestamp   The date and time at which the record was last modified.
	 */
	explicit Identifier(const std::string &identifier, const std::string &last_modification_timestamp = "")
		: identifier_(identifier), last_modification_timestamp_(last_modification_timestamp) { }

	/** Destroy a metadata record. */
	virtual ~Identifier() { }

	/** Get the metadata record identifier. */
	std::string getIdentifier() const
		{ return identifier_; }

	/** Get the metadata last modification timestamp. */
	std::string getLastModificationTimestamp() const
		{ return last_modification_timestamp_; }

	/** Set the metadata last modification timestamp. */
	void setLastModificationTimestamp(const std::string &timestamp)
		{ last_modification_timestamp_ = timestamp; }

private:
	Identifier();  // Intentionally unimplemented!
};


/** \brief  Represents an OAI-PMH record as an identifier with a list of Field values.
 */
class Record: public Identifier {
	/** The fields associted with this record. */
	std::list<Field> fields_;
public:

	/** \brief  Contruct a metadata record.
	 *  \param  identifier                   The record identifier.
	 *  \param  last_modification_timestamp  The date and time at which the record was last modified.
	 */
	explicit Record(const std::string &identifier, const std::string &last_modification_timestamp = "")
		: Identifier(identifier, last_modification_timestamp) { }

	/** \brief  Contruct a metadata record from an Identifier.
	 *  \param  identifier  The record identifier.
	 */
	explicit Record(const Identifier &identifier)
		: Identifier(identifier.getIdentifier(), identifier.getLastModificationTimestamp()) { }

	/** Destroy a metadata record. */
	virtual ~Record() { }

	/** Add a field to the record. */
	void addField(const std::string &name, const std::string &value, const std::string &attribute = "")
		{ fields_.push_back(Field(name, value, attribute)); }

	/** Add a field to the record. */
	void addField(const Field &field) { fields_.push_back(field); }

	/** \brief   Get the list of fields associsted with this record.
	 *  \return  A refernces to the list of fields associated with this record.
	 */
	const std::list<Field> &getFields() const { return fields_; }
private:
	Record(); // Intentionally unimplemented!
};


/** \brief  Represents an OAI-PMH set, comprising a specifier, a name, and a description.
 */
class Set {
	std::string specifier_;
	std::string name_;
	std::string description_;
public:

	/** Contruct an OAI-PMH set. */
	Set(const std::string &specifier, const std::string &name, const std::string &description = "")
		: specifier_(specifier), name_(name), description_(description) { }

	/** Destroy an OAI-PMH set. */
	virtual ~Set() { }

	/** Get the set specifier. */
	std::string getSpecifier() const
		{ return specifier_; }

	/** Get the set name. */
	std::string getName() const
		{ return name_; }

	/** Get the set description. */
	std::string getDescription() const
		{ return description_; }

private:
	Set();                                   // Intentionally unimplemented!
};


/** \brief  Describes the information about a known metadata format.
 */
class MetadataFormat {
	/** The name of this metadata format. */
	std::string name_;

	/** The XML element that will contain the metadata. */
	std::string container_;

	/** The list of namespaces and schema locations output for the XML container element. */
	std::list<std::string> namespaces_and_schema_locations_;

	/*  A map from metadata element names to the corresponding XML tags.
	 *  This class is a map from the names of the fields appearing in a
	 *  metadata record to the XML tags used in OAI-PMH.  This allows us
	 *  to store relationships such as "the local field 'url' maps to the
	 *  XML tag 'identifier'.
	 */
	mutable std::map<std::string, std::string> xml_element_map_;

	/** A map from metadata element names to the corresponding XML
	 * attributes.  The XML attributes always represent OAI-PMH
	 * qualaifiers, and have values like 'dci:type="dci:LCSH"'.
	 */
	mutable std::map<std::string, std::string> xml_attribute_map_;

 public:
	/** Construct a MetadataFormat from a configuration file. */
	explicit MetadataFormat(const std::string &name, const IniFile &ini_file);

	/** Get the name of this metadata format, as requested in the metadataPrefix field. */
	std::string getName() const { return name_; }

	/** Get the XML container element. */
	std::string getContainer() const { return container_; }

	/** Get the XML container's namespaces and schema locations. */
	const std::list<std::string> &getNamespacesAndSchemaLocations() const
		{ return namespaces_and_schema_locations_; }

	/** Get the XML element corresponding to a given field name. */
	std::string getXmlElement(const std::string &field_name) const { return xml_element_map_[field_name]; }

	/** Get the XML attribute corresponding to a given field qualifier. */
	std::string getXmlAttribute(const std::string &field_name) const { return xml_attribute_map_[field_name]; }

private:
	MetadataFormat(); // intentionally unimplemented
};


} // namespace OaiPmh


#endif // ifndef OAI_PMH_H
