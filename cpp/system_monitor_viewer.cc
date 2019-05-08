/** \file   system_monitor_viewer.cc
 *  \brief  Visualises the metrics collected by the system_monitor service
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <ctime>
#include "BinaryIO.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--output-filename=path] system_id metric time_range\n"
            "       system_id       - One of the following: nu ptah sobek ub15 ub16 ub28\n"
            "       metric          - One of the following: mem cpu disk\n"
            "       time_range      - One of the following time ranges:\n"
            "                            YYYY/MM/DD[THH:MM:SS][-YYYY/MM/DD[THH:MM:SS]\n"
            "                            last <n> <hours|days|weeks|months>\n"
            "       The config file path is \"" + UBTools::GetTuelibPath() + FileUtil::GetBasename(::progname) + ".conf\".");
}


bool ParseTimestamp(const std::string &timestamp, struct tm * const tm) {
    return TimeUtil::StringToStructTm(tm, timestamp, "%Y/%m/%d") or
           TimeUtil::StringToStructTm(tm, timestamp, "%Y/%m/%dT%H:%M:%S");
}


void ParseTimeRange(const std::string &range_string, time_t * const time_start, time_t * const time_end) {
    std::vector<std::string> tokens;

    if (StringUtil::StartsWith(range_string, "last ", true)) {
        StringUtil::SplitThenTrimWhite(range_string, ' ', &tokens, true);
        if (tokens.size() != 3)
            LOG_ERROR("invalid time range");

        unsigned time_atom(0);
        if (not StringUtil::ToUnsigned(tokens[1], &time_atom))
            LOG_ERROR("time atom has to be greater than zero");

        auto current_time(std::time(nullptr));
        unsigned long seconds_to_deduct(0);
        const auto granularity(tokens[2]);
        if (StringUtil::StartsWith(granularity, "hour", true))
            seconds_to_deduct = time_atom * 3600;
        else if (StringUtil::StartsWith(granularity, "day", true))
            seconds_to_deduct = time_atom * 24 * 3600;
        else if (StringUtil::StartsWith(granularity, "week", true))
            seconds_to_deduct = time_atom * 7 * 24 * 3600;
        else if (StringUtil::StartsWith(granularity, "month", true))
            seconds_to_deduct = time_atom * 31 * 24 * 3600;
        else
            LOG_ERROR("invalid time range");

        *time_start = current_time - seconds_to_deduct;
        *time_end = current_time;

        return;
    }

    struct tm start_time_buffer, end_time_buffer;
    std::memset(&start_time_buffer, 0, sizeof(struct tm));
    std::memset(&end_time_buffer, 0, sizeof(struct tm));

    if (ParseTimestamp(range_string, &start_time_buffer)) {
        *time_start = std::mktime(&start_time_buffer);
        *time_end = TimeUtil::BAD_TIME_T;
        if (*time_start == TimeUtil::BAD_TIME_T)
            LOG_ERROR("invalid time range");

        return;
    }

    if (StringUtil::Split(range_string, '-', &tokens, true) != 2)
        LOG_ERROR("invalid time range");

    if (not ParseTimestamp(tokens[0], &start_time_buffer) or
        not ParseTimestamp(tokens[1], &end_time_buffer))
    {
        LOG_ERROR("invalid time range");
    }

    *time_start = std::mktime(&start_time_buffer);
    *time_end = std::mktime(&end_time_buffer);
    if (*time_start == TimeUtil::BAD_TIME_T)
        LOG_ERROR("invalid time range");
}


struct Datapoint {
    std::string label_;
    time_t timestamp_;
    std::string value_;

    Datapoint(const std::string &label, const time_t timestamp, const std::string &value)
        : label_(label), timestamp_(timestamp), value_(value) {}

    bool operator<(const Datapoint &rhs) const {
        return this->timestamp_ < rhs.timestamp_;
    }
};


void LoadSystemMonitorLog(const std::string &log_path, const std::unordered_map<uint8_t, std::string> &ordinal_to_label_map,
                          std::vector<Datapoint> * const data)
{
    static constexpr size_t DATA_INITIAL_SIZE = 1000 * 1000;

    File log_file(log_path, "rb");
    int entry_num(1);
    std::vector<std::string> parts;
    data->reserve(DATA_INITIAL_SIZE);

    while (not log_file.eof()) {
        uint32_t timestamp;
        uint8_t ordinal;
        uint32_t value;

        BinaryIO::ReadOrDie(log_file, &timestamp);
        BinaryIO::ReadOrDie(log_file, &ordinal);
        BinaryIO::ReadOrDie(log_file, &value);

        if (ordinal_to_label_map.find(ordinal) == ordinal_to_label_map.end())
            LOG_ERROR("unknown ordinal " + std::to_string(ordinal) + " in log entry " + std::to_string(entry_num));

        data->emplace_back(ordinal_to_label_map.at(ordinal), static_cast<time_t>(timestamp), std::to_string(value));
        ++entry_num;
    }

    // sort by timestamp
    std::sort(data->begin(), data->end());
}


void GetDataRange(const time_t time_start, const time_t time_end, const std::vector<Datapoint> &data,
                  std::vector<Datapoint>::const_iterator * const begin, std::vector<Datapoint>::const_iterator * const end)
{
    // The list should be sorted at this point
    *begin = std::lower_bound(data.begin(), data.end(), Datapoint("", time_start, ""));
    *end = std::upper_bound(data.begin(), data.end(), Datapoint("", time_end, ""));
}


unsigned WritePlotDataToDisk(const std::string &output_path, const std::vector<std::string> &labels,
                         const std::vector<Datapoint>::const_iterator &data_begin, const std::vector<Datapoint>::const_iterator &data_end)
{
    // We expect the values of the labels to use the same axis/scale
    // Columns: Timestamp [Label 1..n]
    File plot_data(output_path, "w");

    plot_data.writeln("#\t" + StringUtil::Join(labels, '\t'));

    time_t current_write_timestamp(TimeUtil::BAD_TIME_T);
    std::map<std::string, std::string> current_write_timestamp_values;
    unsigned lines_written(0);

    for (auto itr(data_begin); itr != data_end; ++itr) {
        const auto datapoint(*itr);

        if (current_write_timestamp == datapoint.timestamp_) {
            current_write_timestamp_values[datapoint.label_] = datapoint.value_;
            continue;
        }

        if (not current_write_timestamp_values.empty()) {
            std::string out_line(std::to_string(current_write_timestamp) + "\t");
            for (const auto &label : labels) {
                const auto value(current_write_timestamp_values.find(label));
                if (value != current_write_timestamp_values.end())
                    out_line += value->second + "\t";
                else
                    out_line += "\t";
            }

            plot_data.writeln(out_line);
            ++lines_written;
        }

        current_write_timestamp = datapoint.timestamp_;
        current_write_timestamp_values[datapoint.label_] = datapoint.value_;
    }

    if (not current_write_timestamp_values.empty()) {
        std::string out_line(std::to_string(current_write_timestamp) + "\t");
        for (const auto &label : labels) {
            const auto value(current_write_timestamp_values.find(label));
            if (value != current_write_timestamp_values.end())
                out_line += value->second + "\t";
            else
                out_line += "\t";
        }

        plot_data.writeln(out_line);
        ++lines_written;
    }

    return lines_written;
}


void DisplayPlot(const std::string &data_path, const std::string &script_path, const std::string &plot_path) {
    if (not FileUtil::Exists(data_path))
        LOG_ERROR("data file for plotting does not exist at " + data_path);
    else if (not FileUtil::Exists(script_path))
        LOG_ERROR("script file for plotting does not exist at " + script_path);

    std::vector<std::string> gnuplot_args {
        "-c",
        script_path,
        data_path,
        plot_path,
    };
    ExecUtil::ExecOrDie("/usr/bin/gnuplot", gnuplot_args);

    std::vector<std::string> xdg_args {
        plot_path
    };

    ExecUtil::ExecOrDie("/usr/bin/xdg-open", xdg_args);
}


const std::set<std::string> VALID_SYSTEM_IDS{
    "nu", "ptah", "sobek", "ub15", "ub16", "ub28"
};


const std::set<std::string> VALID_COARSE_METRICS{
    "cpu", "mem", "disk"
};


void GetLabelsForCoarseMetric(const std::string &coarse_metric, std::vector<std::string> * const labels) {
    // this order needs to be observed in the plotting scripts
    if (coarse_metric == "mem") {
        labels->emplace_back("MemAvailable");
        labels->emplace_back("Unevictable");
        labels->emplace_back("SwapFree");
    } else if (coarse_metric == "cpu")
        labels->emplace_back("CPU");
    else if (coarse_metric == "disk")
        ;
    else
        LOG_ERROR("invalid coarse metric '" + coarse_metric + "'");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    const std::string OUTPUT_FILENAME_FLAG_PREFIX("--output-filename=");
    std::string output_filename;
    if (StringUtil::StartsWith(argv[1], OUTPUT_FILENAME_FLAG_PREFIX)) {
        output_filename = argv[1] + OUTPUT_FILENAME_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    if (argc != 4)
        Usage();

    const std::string system_id(TextUtil::UTF8ToLower(argv[1]));
    const std::string coarse_metric(TextUtil::UTF8ToLower(argv[2]));
    const std::string time_range(argv[3]);

    if (VALID_SYSTEM_IDS.find(system_id) == VALID_SYSTEM_IDS.end())
        LOG_ERROR("invalid system ID '" + system_id + "'");
    else {
        const auto hostname(MiscUtil::SafeGetEnv("HOSTNAME"));
        if (not StringUtil::StartsWith(hostname, system_id, true))
            LOG_WARNING("attempting to view system monitor data of a system that is not the host. time range may be inaccurate");
    }

    time_t time_start, time_end;
    std::vector<Datapoint> log_data;
    std::vector<Datapoint>::const_iterator data_range_start, data_range_end;
    std::vector<std::string> labels;

    ParseTimeRange(time_range, &time_start, &time_end);
    GetLabelsForCoarseMetric(coarse_metric, &labels);

    const IniFile ini_file(UBTools::GetTuelibPath() + FileUtil::GetBasename(::progname) + ".conf");
    const auto log_file(ini_file.getString("Logs", system_id));
    const auto plot_data_file(ini_file.getString("Default Plotting Inputs", coarse_metric));
    const auto plot_script_file(ini_file.getString("Plotting Scripts", coarse_metric));
    if (output_filename.empty())
        output_filename = ini_file.getString("Default Plotting Outputs", coarse_metric);

    std::unordered_map<uint8_t, std::string> ordinal_to_label_map;
    const IniFile monitor_ini_file(UBTools::GetTuelibPath() + "/system_monitor.conf");
    for (const auto &entry : *monitor_ini_file.getSection("Label Ordinals"))
        ordinal_to_label_map[StringUtil::ToUnsigned(entry.name_)] = entry.value_;


    LoadSystemMonitorLog(log_file, ordinal_to_label_map, &log_data);
    GetDataRange(time_start, time_end, log_data, &data_range_start, &data_range_end);

    if (data_range_start == log_data.end())
        LOG_ERROR("found no data that was newer than the given range's beginning");

    if (time_end == TimeUtil::BAD_TIME_T) {
        // print out the closest data point
        if (data_range_start->timestamp_ == time_start)
            LOG_INFO("Data for exact time point (" + TimeUtil::TimeTToString(time_start) + "):");
        else
            LOG_INFO("Data for closest time point (" + TimeUtil::TimeTToString(time_start) + "):");

        const auto datapoint_timestamp(data_range_start->timestamp_);
        while (data_range_start != log_data.end() and data_range_start->timestamp_ == datapoint_timestamp)
            LOG_INFO("\t" + data_range_start->label_ + " = " + data_range_start->value_);

        return EXIT_SUCCESS;
    }

    if (WritePlotDataToDisk(plot_data_file, labels, data_range_start, data_range_end) == 0)
        LOG_WARNING("found no data for the given time range");
    else
        DisplayPlot(plot_data_file, plot_script_file, output_filename);

    return EXIT_SUCCESS;
}
