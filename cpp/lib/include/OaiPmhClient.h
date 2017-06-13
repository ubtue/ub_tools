/** \file    OaiPmhClient.h
 *  \brief   Declaration of the OaiPmh::Client class, representing an OAI-PMH client.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 *
 */

/*
 *  Copyright 2003-2005 Project iVia.
 *  Copyright 2003-2005 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaOaiPmh package.
 *
 *  The libiViaOaiPmh package is free software; you can redistribute
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

#ifndef OAI_PMH_CLIENT_H
#define OAI_PMH_CLIENT_H


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
#ifndef VECTOR
#       include <vector>
#       define VECTOR
#endif
#ifndef OAI_PMH_H
#       include "OaiPmh.h"
#endif


// Forward declarations:
class IniFile;
class Logger;


namespace OaiPmh {


/** \brief  A base clase for implementing an OAI-PMH version 2 Client.
 *
 *  The OaiPmh::Client class is a base class that can be extended with
 *  a subclass to build an OAI-PMH client.  The base class handles
 *  most of the complexity of the OAI-PMH protocol, while the subclass
 *  will include member functions for handling the harvested metadata.
 *
 *  \par Specialised subclasses
 *
 *  The OaiPmh::Client class is a base class for harvesting records
 *  from OAI-PMH servers.  To implement a client programs, you need to
 *  create a specialised OaiPmh::Client subclass that implements
 *  OaiPmh::Client::processRecord() to handle the imported records
 *  appropriately. To perform an import, instantiate the class and
 *  call the harvest() member function.
 */
class Client {
protected:
    /** The name of the repository we will harvest records from. */
    std::string repository_name_;

    /** The base URL for the repository. */
    std::string base_url_;

    /** The list of known sets at the repository. */
    std::list<std::string> sets_;

    /** Do we perform a full or incremental harvest (default: INCREMENTAL)? */
    HarvestMode harvest_mode_;

    /** The metadataPrefix argument to use during the harvest. */
    std::string metadata_prefix_;

    /** The date that the first response was returned in this run of the program. */
    std::string first_response_date_;

public:
    struct MetadataFormatDescriptor {
        std::string metadata_prefix_, schema_, metadata_namespace_;
    public:
        std::string toString() const;
    };
public:
    /** \brief  Construct a Client object based on a configuration file.
     *  \param  ini_file       The IniFile object.
     *  \param  section_name   The name of the section where the harvest is defined.
     */
    Client(const IniFile &ini_file, const std::string &section_name);

    /** \brief  Destroy an OAI-PMH client instance. */
    virtual ~Client();

    /** \brief Change the harvest mode. */
    void setHarvestMode(const HarvestMode harvest_mode) { harvest_mode_ = harvest_mode; }

    /** \brief Enumerate the server's supported metadata formats.
     *  \param metadata_format_list  Where the list of supported formats will be returned.
     *  \param error_message         If the request fails an explanation will be found here.
     *  \param identifier            An optional argument that specifies the unique identifier of the item for which
     *                               available metadata formats are being requested.
     *  \return True if we successfully retrieved the list, o/w false.
     */
    bool listMetadataFormats(std::vector<MetadataFormatDescriptor> * const metadata_format_list,
                             std::string * const error_message, const std::string &identifier = "");

    /** \brief  Harvest a single set.
     *  \param  set_name       The name of the set to harvest.
     *  \param  verbosity      The quantity of log messages (0 = none, 3 = normal, 5 = too much).
     *  \param  logger         A logger object, or NULL for no logging.
     */
    void harvest(const std::string &set_name, const unsigned verbosity, Logger * const logger);

    /** \brief  Harvest the list of known sets.
     *  \param  logger         A logger object, or NULL for no logging.
     *  \param  verbosity      The quantity of log messages (0 = none, 3 = normal, 5 = too much).
     */
    void harvest(const unsigned verbosity, Logger * const logger);

    /** \brief   Retrieve the repository's XML response to an Identify query.
     *  \param   xml_response    Output variable which will hold the XML returned.
     *  \param   error_message  Output variable which will hold any error encountered.
     *  \return  True if a useful response was discovered, otherwise false.
     */
    bool identify(std::string * const xml_response, std::string * const error_message);

protected:
    /** \brief   Get the filename of a progress file for a particular set.
     *  \param   set_name  The name of the set being harvested.
     *  \return  The absolute path of the progress file.
     *
     *  The progress file is used to implement incremental
     *  harvests.  It will be used to store the last harvest date
     *  for the given set.  It defaults to
     *  "/tmp/[client_program].[repository].[set_name].progress"
     *
     *  This function SHOULD be implemented by Subclasses to use a
     *  directory other than /tmp.
     */
    virtual std::string progressFile(const std::string &set_name);

    /** \brief   Process a single record that has been imported by the client.
     *  \param   record     An OAI-PMH record that was read from the repository.
     *  \param   verbosity  The quantity of log messages (0 = none, 3 = normal, 5 = too much).
     *  \param   logger     A logger object, or NULL for no logging.
     *  \return  True if the record was imported, false if for any reason it was not.
     *
     *  This function MUST be implemented by subclasses.  It is
     *  called once for each record harvested from the OAI-PMH
     *  client, and its purpose is to take the imported record and
     *  store it in a method appropriate to the local application.
     *
     *  \note  The return value is only used to maintain statistics
     *         about the imported records and has no effect on the
     *         progress of the harvest.
     */
    virtual bool processRecord(const Record &record, const unsigned verbosity, Logger * const logger) = 0;

private:
    /** \brief  Harvest a specific set. */
    void harvestSet(const std::string &set_spec, const std::string &from, const std::string &until,
                    const unsigned verbosity, Logger * const logger);
};


} // namespace OaiPmh


#endif // ifndelf OAI_PMH_CLIENT_H
