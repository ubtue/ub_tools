#include "PhaseDeleteUnusedLocalData.h"
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "RegexMatcher.h"
#include "util.h"


static ssize_t before_count(0), after_count(0);


bool IsUnusedLocalBlock(const MarcUtil::Record &record, const std::pair <size_t, size_t> &block_start_and_end) {
    static RegexMatcher *matcher(nullptr);
    std::string err_msg;
    if (unlikely(matcher == nullptr)) {
        matcher = RegexMatcher::RegexMatcherFactory("^.*aDE-21.*$|^.*aDE-21-24.*$|^.*aDE-21-110.*$|^.*aTü 135.*$", &err_msg);
        if (matcher == nullptr)
            Error(err_msg);
    }

    std::vector <size_t> field_indices;
    record.findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);

    const std::vector <std::string> &fields(record.getFields());
    for (const auto field_index : field_indices) {
        const bool matched = matcher->matched(fields[field_index], &err_msg);
        if (not matched and not err_msg.empty())
            Error("Unexpected error while trying to match a field in IsUnusedLocalBlock: " + err_msg);
        if (matched)
            return false;
    }
    return true;
}


void DeleteLocalBlock(MarcUtil::Record &record, const std::pair <size_t, size_t> &block_start_and_end) {
    for (size_t field_index(block_start_and_end.second - 1); field_index >= block_start_and_end.first; --field_index)
        record.deleteField(field_index);
}


PipelinePhaseState PhaseDeleteUnusedLocalData::process(MarcUtil::Record &record, std::string *const) {
    std::vector <std::pair<size_t, size_t>> local_block_boundaries;
    ssize_t local_data_count = record.findAllLocalDataBlocks(&local_block_boundaries);
    std::reverse(local_block_boundaries.begin(), local_block_boundaries.end());

    before_count += local_data_count;
    for (const std::pair <size_t, size_t> &block_start_and_end : local_block_boundaries) {
        if (IsUnusedLocalBlock(record, block_start_and_end)) {
            DeleteLocalBlock(record, block_start_and_end);
            --local_data_count;
        }
    }

    after_count += local_data_count;
    return SUCCESS;
};


PhaseDeleteUnusedLocalData::~PhaseDeleteUnusedLocalData() {
    std::cerr << "Delete unused local data:\n";
    std::cerr << "\t" << ": Deleted " << (before_count - after_count) << " of " << before_count << " local data blocks.\n";
};