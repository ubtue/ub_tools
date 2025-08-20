/** \brief Determine the type of SSGN augmentation for superior works based on SSG of inferior works
 *
 *  \copyright 2023 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "FileUtil.h"
#include "MARC.h"
#include "Solr.h"
#include "util.h"


enum SSGN { ONE, ZERO };
using ppn_to_inferiors_map = std::unordered_map<std::string, std::set<std::string>>;
using inferiors_to_ssgns_map = std::unordered_map<std::string, std::set<SSGN>>;
using ppn_to_inferiors_with_ssgns_map = std::unordered_map<std::string, inferiors_to_ssgns_map>;

namespace {


void CreatePPNToInferiorsMap(const std::string &solr_host, const unsigned solr_port, File * const superiors_without_ssgn,
                             ppn_to_inferiors_map * const ppns_to_inferiors) {
    std::cerr << "Creating PPNtoInferiorsMap...\n";
    while (not superiors_without_ssgn->eof()) {
        std::string ppn;
        superiors_without_ssgn->getline(&ppn);
        std::string json_result;
        std::string err_msg;
        if (not Solr::Query("superior_ppn:" + ppn, "id", 0, 1000000, &json_result, &err_msg, solr_host, solr_port, Solr::DEFAULT_TIMEOUT,
                            Solr::QueryResultFormat::JSON))
            LOG_ERROR("Query failed: " + err_msg);
        const auto result(nlohmann::json::parse(json_result));
        std::set<std::string> inferior_ppns;
        for (const auto &doc : result["response"]["docs"]) {
            const std::string id(doc["id"]);
            inferior_ppns.emplace(id);
        }
        ppns_to_inferiors->emplace(ppn, inferior_ppns);
    }
    std::cerr << "Finished\n";
}


void GetSSGNsForInferiors(MARC::Reader * const marc_reader, const ppn_to_inferiors_map &ppns_to_inferiors,
                          inferiors_to_ssgns_map * const all_inferiors_to_ssgns) {
    std::cerr << "Get SSGNsForInferiors...\n";
    // Collect all inferiors for all superior ppns
    std::unordered_set<std::string> all_inferiors;
    for (const auto &[ppn, inferiors] : ppns_to_inferiors)
        all_inferiors.insert(inferiors.begin(), inferiors.end());

    // Determine the SSGNs for all inferiors
    while (MARC::Record record = marc_reader->read()) {
        if (all_inferiors.contains(record.getControlNumber())) {
            const auto all_ssgns(record.getSSGNs());
            std::set<SSGN> ssgns;
            for (const auto &ssgn : all_ssgns) {
                if (ssgn == "1")
                    ssgns.emplace(ONE);
                else if (ssgn == "0")
                    ssgns.emplace(ZERO);
            }
            all_inferiors_to_ssgns->emplace(record.getControlNumber(), ssgns);
        }
    }
    std::cerr << "Finished\n";
}

// Create new mapping but include only inferiors with existing 0/1-SSGNs
void CreatePPNsToInferiorsMapWithSSGNs(const ppn_to_inferiors_map &ppns_to_inferiors, const inferiors_to_ssgns_map &all_inferiors_to_ssgns,
                                       ppn_to_inferiors_with_ssgns_map * const ppns_to_inferiors_with_ssgns) {
    std::cerr << "Create PPNsToInferiorMapWithSSGNs...\n";
    for (const auto &[ppn, inferiors] : ppns_to_inferiors) {
        inferiors_to_ssgns_map inferiors_to_ssgns;
        for (const auto &inferior : inferiors) {
            const auto inferior_to_ssgns(all_inferiors_to_ssgns.find(inferior));
            if (inferior_to_ssgns != all_inferiors_to_ssgns.end() and inferior_to_ssgns->second.size())
                inferiors_to_ssgns.emplace(inferior, inferior_to_ssgns->second);
        }
        ppns_to_inferiors_with_ssgns->emplace(ppn, inferiors_to_ssgns);
    }
    std::cerr << "Finished\n";
}


void CreateSSGNLists(const std::string &ssgn_lists_basename, const ppn_to_inferiors_with_ssgns_map &ppns_to_inferiors_with_ssgns) {
    std::cerr << "Creating Outfiles...\n";
    enum OUTFILE_OFFSET { ZERO, ONE, ZERO_ONE };
    const std::map<OUTFILE_OFFSET, std::string> suffices({ { ZERO, "zero" }, { ONE, "one" }, { ZERO_ONE, "zero_one" } });
    std::vector<std::unique_ptr<File>> outfiles;

    for (const auto &[_, suffix] : suffices) {
        std::unique_ptr<File> outfile(FileUtil::OpenOutputFileOrDie(ssgn_lists_basename + suffix + ".txt"));
        outfiles.push_back(std::move(outfile));
    }

    for (const auto &[ppn, inferiors_with_ssgns] : ppns_to_inferiors_with_ssgns) {
        std::set<SSGN> occurring_ssgns;
        for (const auto &[_, inferior_ssgns] : inferiors_with_ssgns) {
            for (const auto &ssgn : inferior_ssgns) {
                occurring_ssgns.emplace(ssgn);
            }
        }
        if (occurring_ssgns.size() == 1 and *(occurring_ssgns.begin()) == SSGN::ZERO)
            outfiles[ZERO]->write(ppn + '\n');
        else if (occurring_ssgns.size() == 1 and *(occurring_ssgns.begin()) == SSGN::ONE)
            outfiles[ONE]->write(ppn + '\n');
        else if (occurring_ssgns.contains(SSGN::ZERO) and occurring_ssgns.contains(SSGN::ONE))
            outfiles[ZERO_ONE]->write(ppn + '\n');
    }
    std::cerr << "Finished\n";
}


void GetSolrHostAndPort(std::string *solr_host_and_port, std::string * const solr_host, unsigned * const solr_port) {
    if (not StringUtil::Contains(*solr_host_and_port, ":"))
        LOG_ERROR("Invalid solr_host:port specification " + *solr_host_and_port);
    *solr_host = StringUtil::ExtractHead(solr_host_and_port, ":");
    *solr_port = StringUtil::ToUnsignedShort(*solr_host_and_port);
}


} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        ::Usage("marc_input solr_host:port, superiors_without_ssgn ssgn_lists_basename");

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));

    std::string solr_host_and_port(argv[2]);
    std::string solr_host;
    unsigned solr_port;
    GetSolrHostAndPort(&solr_host_and_port, &solr_host, &solr_port);

    const std::unique_ptr<File> superiors_without_ssgn(FileUtil::OpenInputFileOrDie(argv[3]));
    const std::string ssgn_lists_basename(argv[4]);


    ppn_to_inferiors_map ppns_to_inferiors;
    CreatePPNToInferiorsMap(solr_host, solr_port, superiors_without_ssgn.get(), &ppns_to_inferiors);

    inferiors_to_ssgns_map all_inferiors_to_ssgns;
    GetSSGNsForInferiors(marc_reader.get(), ppns_to_inferiors, &all_inferiors_to_ssgns);

    ppn_to_inferiors_with_ssgns_map ppns_to_inferiors_with_ssgns;
    CreatePPNsToInferiorsMapWithSSGNs(ppns_to_inferiors, all_inferiors_to_ssgns, &ppns_to_inferiors_with_ssgns);

    CreateSSGNLists(ssgn_lists_basename, ppns_to_inferiors_with_ssgns);

    return EXIT_SUCCESS;
}
