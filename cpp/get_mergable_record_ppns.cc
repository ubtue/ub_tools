/** \brief Utility to enumerate mergable records
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdlib>
#include "BSZUtil.h"
#include "ControlNumberGuesser.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] marc_input mergable_records_list\n";
    std::exit(EXIT_FAILURE);
}


// A set of records that can potentially be merged into a single record.
struct CandidateMergeSet {
    std::set<std::string> mergable_ppns_;
};


inline bool operator<(const CandidateMergeSet &lhs, const CandidateMergeSet &rhs) {
    return lhs.mergable_ppns_ < rhs.mergable_ppns_;
}


// Additional data which should match between the candidate records.
struct MergeConstraintsData {
    std::string year_;
    std::string volume_;
    std::string book_;

    MergeConstraintsData(const MARC::Record &record) {
        BSZUtil::ExtractYearVolumeBook(record, &year_, &volume_, &book_);
    }
};


inline bool operator==(const MergeConstraintsData &lhs, const MergeConstraintsData &rhs) {
    return lhs.year_ == rhs.year_ and lhs.volume_ == rhs.volume_ and lhs.book_ == rhs.book_;
}


inline bool operator!=(const MergeConstraintsData &lhs, const MergeConstraintsData &rhs) {
    return not operator==(lhs, rhs);
}


// Maps PPNs to sets of other PPNs with which they can be potentially merged.
// Each PPN can potentially belong to multiple merge sets.
using PpnToMergeSets = std::unordered_multimap<std::string, CandidateMergeSet>;
using PpnToMergeConstraintsData = std::unordered_map<std::string, MergeConstraintsData>;


void EnumerateMergableRecords(const ControlNumberGuesser &control_number_guesser, MARC::Reader * const reader,
                              PpnToMergeSets * const ppn_to_merge_sets, PpnToMergeConstraintsData * const ppn_to_merge_constraints_data)
{
    while (const MARC::Record record = reader->read()) {
        const auto ppn(record.getControlNumber());
        const auto author_names(record.getAllAuthors());
        const CandidateMergeSet guessed_control_numbers {
            control_number_guesser.getGuessedControlNumbers(record.getCompleteTitle(),
                                                            std::vector<std::string>(author_names.begin(), author_names.end()))
        };

        if (ppn_to_merge_constraints_data->find(ppn) != ppn_to_merge_constraints_data->end())
            LOG_ERROR("duplicate PPN '" + ppn + "'!");

        ppn_to_merge_constraints_data->insert(std::make_pair(ppn, MergeConstraintsData(record)));

        // a unique record, moving on
        if (guessed_control_numbers.mergable_ppns_.empty())
            continue;

        auto candidate_merge_set_range(ppn_to_merge_sets->equal_range(ppn));
        bool merge_set_exists(false);
        for (auto merge_set(candidate_merge_set_range.first); merge_set != candidate_merge_set_range.second; ++merge_set) {
            if (guessed_control_numbers.mergable_ppns_ == merge_set->second.mergable_ppns_) {
                merge_set_exists = true;
                break;
            }
        }

        if (not merge_set_exists)
            ppn_to_merge_sets->insert(std::make_pair(ppn, guessed_control_numbers));
    }
}


// Remove any candidate record from the merge set that doesn't match the canonical data.
void ApplySecondaryMergeConstraints(const PpnToMergeConstraintsData &ppn_to_merge_constraints_data, PpnToMergeSets * const ppn_to_merge_sets) {
    for (auto &entry : *ppn_to_merge_sets) {
        const auto &ppn(entry.first);
        auto &merge_set(entry.second);

        const auto &canonical_merge_constraints_data(*ppn_to_merge_constraints_data.find(ppn));
        for (auto mergable_ppn(merge_set.mergable_ppns_.begin());
             mergable_ppn != merge_set.mergable_ppns_.end();
             /* intentionally empty */)
        {
            const auto &candidate_merge_constraints_data(ppn_to_merge_constraints_data.find(*mergable_ppn));
            if (candidate_merge_constraints_data == ppn_to_merge_constraints_data.end())
                LOG_ERROR("couldn't find merge constraints data for candidate ppn '" + *mergable_ppn + "'");

            if (canonical_merge_constraints_data.second != candidate_merge_constraints_data->second) {
                mergable_ppn = merge_set.mergable_ppns_.erase(mergable_ppn);
                continue;
            }
        }
    }
}


// Deduplicate and merge the candidate merge sets.
void CollateMergeSets(PpnToMergeSets * const ppn_to_merge_sets, std::set<CandidateMergeSet> * const collated_merge_sets,
                      std::unordered_map<std::string, int> * const ppns_with_multiple_merge_sets)
{
    std::set<std::string> collated_source_ppns;

    for (auto entry(ppn_to_merge_sets->begin()); entry != ppn_to_merge_sets->end(); /* intentionally empty */) {
        const auto &ppn(entry->first);
        auto &merge_set(entry->second);

        if (merge_set.mergable_ppns_.size() < 2) {
            if (merge_set.mergable_ppns_.size() == 1 and *merge_set.mergable_ppns_.begin() != ppn)
                LOG_ERROR("candidate merge set has a single entry that isn't the source ppn '" + ppn + "'");

            entry = ppn_to_merge_sets->erase(entry);
            continue;
        }

        if (collated_merge_sets->find(merge_set) != collated_merge_sets->end())
            collated_merge_sets->insert(merge_set);

        if (collated_source_ppns.find(ppn) == collated_source_ppns.end())
            collated_source_ppns.insert(ppn);
        else if (ppns_with_multiple_merge_sets->find(ppn) == ppns_with_multiple_merge_sets->end())
            ppns_with_multiple_merge_sets->insert(std::make_pair(ppn, 2));
        else
            (*ppns_with_multiple_merge_sets)[ppn] += 1;
    }
}


void WriteMergableRecordList(const std::set<CandidateMergeSet> &collated_merge_sets, File * const output_file) {
    for (const auto &merge_set : collated_merge_sets) {
        std::string merge_list;
        for (const auto &mergabel_ppn : merge_set.mergable_ppns_)
            merge_list += mergabel_ppn + ",";

        if (not merge_list.empty())
            merge_list.erase(merge_list.length() - 1);

        if (not merge_list.empty())
            *output_file << merge_list << "\n";
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    auto input_file(MARC::Reader::Factory(argv[1]));
    File output_file(argv[2], "w");

    PpnToMergeSets ppn_to_merge_sets;
    PpnToMergeConstraintsData ppn_to_merge_constraints_data;
    std::set<CandidateMergeSet> collated_merge_sets;
    std::unordered_map<std::string, int> ppns_with_multiple_merge_sets;
    ControlNumberGuesser control_number_guesser(ControlNumberGuesser::DO_NOT_CLEAR_DATABASES);

    EnumerateMergableRecords(control_number_guesser, input_file.get(), &ppn_to_merge_sets, &ppn_to_merge_constraints_data);
    ApplySecondaryMergeConstraints(ppn_to_merge_constraints_data, &ppn_to_merge_sets);
    CollateMergeSets(&ppn_to_merge_sets, &collated_merge_sets, &ppns_with_multiple_merge_sets);
    WriteMergableRecordList(collated_merge_sets, &output_file);

    LOG_INFO("Number of candidate merge sets: " + std::to_string(collated_merge_sets.size()));
    LOG_INFO("Number of PPNs with multiple merge sets: " + std::to_string(ppns_with_multiple_merge_sets.size()));

    return EXIT_SUCCESS;
}
