#ifndef SMART_DOWNLOADER_H
#define SMART_DOWNLOADER_H


#include <memory>
#include <string>
#include <vector>
#include "RegexMatcher.h"


class SmartDownloader {
    std::unique_ptr<RegexMatcher> matcher_;
    unsigned success_count_;
public:
    explicit SmartDownloader(const std::string &regex);
    virtual ~SmartDownloader() { }

    virtual std::string getName() const = 0;

    /** \return True if this is the correct downloader for "url", else false. */
    virtual bool canHandleThis(const std::string &url) const;

    /** \brief Attempt to download a document from "url".
     *  \param url       Where to get our document or at least a landing page that will hopefully lead us
     *                   to the document.
     *  \param timeout   How long we aree maximally willing to wait for each download phase, in seconds.
     *  \param document  Where to return the downloaded document.
     *  \return True if we succeeded in downloading the document, else false.
     */
    bool downloadDoc(const std::string &url, const unsigned timeout, std::string * const document);

    /** \return How often downloadDoc() returned true. */
    unsigned getSuccessCount() const { return success_count_; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document) = 0;
};


/** \class SimpleSuffixDownloader
 *  \brief Accepts all URLs that case-insensitively end in one of the suffixes passed into the constructor.
 */
class SimpleSuffixDownloader: public SmartDownloader {
    const std::vector<std::string> suffixes_;
public:
    explicit SimpleSuffixDownloader(std::vector<std::string> &&suffixes)
	: SmartDownloader(""), suffixes_(suffixes) { }

    virtual std::string getName() const { return "SimpleSuffixDownloader"; }
    virtual bool canHandleThis(const std::string &url) const;
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


/** \class SimplePrefixDownloader
 *  \brief Accepts all URLs that case-insensitively start with one of the prefixes passed into the constructor.
 */
class SimplePrefixDownloader: public SmartDownloader {
    const std::vector<std::string> prefixes_;
public:
    explicit SimplePrefixDownloader(std::vector<std::string> &&prefixes)
	: SmartDownloader(""), prefixes_(prefixes) { }

    virtual std::string getName() const { return "SimplePrefixDownloader"; }
    virtual bool canHandleThis(const std::string &url) const;
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


class DigiToolSmartDownloader: public SmartDownloader {
public:
    DigiToolSmartDownloader()
	: SmartDownloader("http://digitool.hbz-nrw.de:1801/webclient/DeliveryManager\\?pid=\\d+") { }
    virtual std::string getName() const { return "DigiToolSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


class IdbSmartDownloader: public SmartDownloader {
public:
    IdbSmartDownloader(): SmartDownloader("http://idb.ub.uni-tuebingen.de/diglit/.+") { }
    virtual std::string getName() const { return "IdbSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


class BszSmartDownloader: public SmartDownloader {
public:
    BszSmartDownloader(): SmartDownloader("http://swbplus.bsz-bw.de/bsz.*\\.htm") { }
    virtual std::string getName() const { return "BszSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


class BvbrSmartDownloader: public SmartDownloader {
public:
    BvbrSmartDownloader(): SmartDownloader("http://bvbr.bib-bvb.de:8991/.+") { }
    virtual std::string getName() const { return "BvbrSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


class Bsz21SmartDownloader: public SmartDownloader {
public:
    Bsz21SmartDownloader(): SmartDownloader("http://nbn-resolving.de/urn:nbn:de:bsz:21.+") { }
    virtual std::string getName() const { return "Bsz21SmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


class LocGovSmartDownloader: public SmartDownloader {
public:
    LocGovSmartDownloader(): SmartDownloader("http://www.loc.gov/catdir/.+") { }
    virtual std::string getName() const { return "LocGovSmartDownloader"; }
protected:
    virtual bool downloadDocImpl(const std::string &url, const unsigned timeout, std::string * const document);
};


#endif // ifndef SMART_DOWNLOADER_H
