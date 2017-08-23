/** \brief Reader for Marc files.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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

#include "FileUtil.h"
#include "MarcReader.h"
#include "MediaTypeUtil.h"


MarcRecord BinaryMarcReader::read() {
    MarcRecord current_record(MarcRecord::ReadSingleRecord(input_));
    if (not current_record)
        return current_record;

    bool is_multi_part(current_record.getLeader().isMultiPartRecord());
    while (is_multi_part) {
        const MarcRecord next_record(MarcRecord::ReadSingleRecord(input_));
        current_record.combine(next_record);
        is_multi_part = next_record.getLeader().isMultiPartRecord();
    }
    return current_record;
}


std::unique_ptr<MarcReader> MarcReader::Factory(const std::string &input_filename, ReaderType reader_type) {
    if (reader_type == AUTO) {
        const std::string media_type(MediaTypeUtil::GetFileMediaType(input_filename));
        if (unlikely(media_type == "cannot"))
            Error("not found or no permissions: \"" + input_filename + "\"!");
        if (unlikely(media_type.empty()))
            Error("can't determine media type of \"" + input_filename + "\"!");
        if (media_type != "application/xml" and media_type != "application/marc")
            Error("\"" + input_filename + "\" is neither XML nor MARC-21 data!");
        reader_type = (media_type == "application/xml") ? XML : BINARY;
    }

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    return (reader_type == XML) ? std::unique_ptr<MarcReader>(new XmlMarcReader<File>(input.release()))
                                : std::unique_ptr<MarcReader>(new BinaryMarcReader(input.release()));
}
