/** Test harness for dealing with the most common domain names.

  10535 http://swbplus.bsz-bw.de                  Done!
   4774 http://digitool.hbz-nrw.de:1801           Done!
   2977 http://www.gbv.de                         PDF's
   1070 http://bvbr.bib-bvb.de:8991               Done!
    975 http://deposit.d-nb.de                    HTML
    772 http://d-nb.info                          PDF's (Images => Need to OCR this?)
    520 http://www.ulb.tu-darmstadt.de            (Frau Gwinner arbeitet daran?)
    236 http://media.obvsg.at                     HTML
    167 http://www.loc.gov
    133 http://deposit.ddb.de
    127 http://www.bibliothek.uni-regensburg.de
     57 http://nbn-resolving.de
     43 http://www.verlagdrkovac.de
     35 http://search.ebscohost.com
     25 http://idb.ub.uni-tuebingen.de
     22 http://link.springer.com
     18 http://heinonline.org
     15 http://www.waxmann.com
     13 https://www.destatis.de
     10 http://www.tandfonline.com
     10 http://dx.doi.org
      9 http://tocs.ub.uni-mainz.de
      8 http://www.onlinelibrary.wiley.com
      8 http://bvbm1.bib-bvb.de
      6 http://www.wvberlin.de
      6 http://www.jstor.org
      6 http://www.emeraldinsight.com
      6 http://www.destatis.de
      5 http://www.univerlag.uni-goettingen.de
      5 http://www.sciencedirect.com
      5 http://www.netread.com
      5 http://www.gesis.org
      5 http://content.ub.hu-berlin.de

 */
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "FileUtil.h"
#include "SmartDownloader.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--timeout seconds] [--trace] url output_filename\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc < 3)
        Usage();

    const unsigned DEFAULT_TIMEOUT(20); // seconds
    unsigned timeout(DEFAULT_TIMEOUT);
    if (std::strcmp(argv[1], "--timeout") == 0) {
        if (not StringUtil::ToUnsigned(argv[2], &timeout))
            Error("bad timeout \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    if (argc < 3)
        Usage();

    bool trace(false);
    if (std::strcmp(argv[1], "--trace") == 0) {
        trace = true;
        --argc;
        ++argv;
    }

    if (argc != 3)
        Usage();

    const std::string url(argv[1]);
    const std::string output_filename(argv[2]);

    try {
        std::string document;
        if (not SmartDownload(url, timeout, &document, trace)) {
            std::cerr << progname << ": Download failed!\n";
            std::exit(EXIT_FAILURE);
        }

        if (not FileUtil::WriteString(output_filename, document)) {
            std::cerr << progname << ": failed to write downloaded document to \"" + output_filename + "\"!\n";
            std::exit(EXIT_FAILURE);
        }
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
