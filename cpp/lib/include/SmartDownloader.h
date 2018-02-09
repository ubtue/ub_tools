/** \file   SmartDownloader.h
 *  \brief  Class hierarchy for matching of URL types and adapting of download behaviours.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbiblothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef SMART_DOWNLOADER_H
#define SMART_DOWNLOADER_H


#include <memory>
#include <string>
#include <vector>
#include "RegexMatcher.h"
#include "TimeLimit.h"


class SmartDownloader {
    std::unique_ptr<RegexMatcher> matcher_;
    unsigned success_count_;
protected:
    bool trace_;
public:
    explicit SmartDownloader(const std::string &regex, const bool trace = false);
    virtual ~SmartDownloader() { }

    virtual std::string getName() const = 0;

    /** \return True if this is the correct downloader for "url", else false. */
    virtual bool canHandleThis(const std::string &url) const;

    /** \brief Attempt to download a document from "url".
     *  \param url         Where to get our document or at least a landing page that will hopefully lead us
     *                     to the document.
     *  \param time_limit  How long we are willing to wait for the complete download operation, in milliseconds.
     *  \param document    Where to return the downloaded document.
     *  \return True if we succeeded in downloading the document, else false.
     */
    bool downloadDoc(const std::string &url, const TimeLimit time_limit, std::string * const document);

    /** \return How often downloadDoc() returned true. */
    unsigned getSuccessCount() const { return success_count_; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                 std::string * const document) = 0;
};


/** \class DSpaceDownloader
 *  \brief Handles DSpace documents
 */
class DSpaceDownloader: public SmartDownloader {
public:
    explicit DSpaceDownloader(const bool trace = false): SmartDownloader("", trace) { }

    virtual std::string getName() const { return "DSpaceDownloader"; }
    virtual bool canHandleThis(const std::string &url) const;
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


/** \class SimpleSuffixDownloader
 *  \brief Accepts all URLs that case-insensitively end in one of the suffixes passed into the constructor.
 */
class SimpleSuffixDownloader: public SmartDownloader {
    const std::vector<std::string> suffixes_;
public:
    explicit SimpleSuffixDownloader(std::vector<std::string> &&suffixes, const bool trace = false)
        : SmartDownloader("", trace), suffixes_(suffixes) { }

    virtual std::string getName() const { return "SimpleSuffixDownloader"; }
    virtual bool canHandleThis(const std::string &url) const;
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


/** \class SimplePrefixDownloader
 *  \brief Accepts all URLs that case-insensitively start with one of the prefixes passed into the constructor.
 */
class SimplePrefixDownloader: public SmartDownloader {
    const std::vector<std::string> prefixes_;
public:
    explicit SimplePrefixDownloader(std::vector<std::string> &&prefixes, const bool trace = false)
        : SmartDownloader("", trace), prefixes_(prefixes) { }

    virtual std::string getName() const { return "SimplePrefixDownloader"; }
    virtual bool canHandleThis(const std::string &url) const;
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


class DigiToolSmartDownloader: public SmartDownloader {
public:
    explicit DigiToolSmartDownloader(const bool trace = false)
        : SmartDownloader("^http://digitool.hbz-nrw.de:1801/webclient/DeliveryManager\\?pid=\\d+.*$", trace) { }
    virtual std::string getName() const { return "DigiToolSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


class DiglitSmartDownloader: public SmartDownloader {
public:
    explicit DiglitSmartDownloader(const bool trace = false)
        : SmartDownloader("^http://idb.ub.uni-tuebingen.de/diglit/.+$|^http://nbn-resolving.de/urn:nbn:de:bsz:21-dt-\\d+$",
                          trace) { }
    virtual std::string getName() const { return "DiglitSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


class BszSmartDownloader: public SmartDownloader {
public:
    explicit BszSmartDownloader(const bool trace = false): SmartDownloader("http://swbplus.bsz-bw.de/bsz.*\\.htm", trace) { }
    virtual std::string getName() const { return "BszSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


class BvbrSmartDownloader: public SmartDownloader {
public:
    explicit BvbrSmartDownloader(const bool trace = false): SmartDownloader("http://bvbr.bib-bvb.de:8991/.+", trace) { }
    virtual std::string getName() const { return "BvbrSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


class Bsz21SmartDownloader: public SmartDownloader {
public:
    explicit Bsz21SmartDownloader(const bool trace = false)
        : SmartDownloader("http://nbn-resolving.de/urn:nbn:de:bsz:21.+", trace) { }
    virtual std::string getName() const { return "Bsz21SmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


class LocGovSmartDownloader: public SmartDownloader {
public:
    explicit LocGovSmartDownloader(const bool trace = false): SmartDownloader("http://www.loc.gov/catdir/.+", trace) { }
    virtual std::string getName() const { return "LocGovSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const TimeLimit time_limit, std::string * const document);
};


/** \brief Tries to download "document" from "url".  Returns if the download succeeded or not. */
bool SmartDownload(const std::string &url, const unsigned max_download_time, std::string * const document,
                   const bool trace = false);


#endif // ifndef SMART_DOWNLOADER_H
