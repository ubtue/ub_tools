/** \brief Utility for topological sorting of nodes in a directed graph.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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

#include <iostream>
#include <unordered_map>
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


inline unsigned MapVertexNameToVertexID(const std::string &vertex_name,
                                        std::unordered_map<unsigned, std::string> * const vertex_id_to_vertex_name_map) {
    static unsigned next_id;
    static std::unordered_map<std::string, unsigned> vertex_name_to_vertex_id_map;

    const auto name_and_id(vertex_name_to_vertex_id_map.find(vertex_name));
    if (name_and_id != vertex_name_to_vertex_id_map.cend())
        return name_and_id->second;

    vertex_name_to_vertex_id_map[vertex_name] = next_id;
    (*vertex_id_to_vertex_name_map)[next_id] = vertex_name;
    ++next_id;
    return next_id - 1;
}


void LoadVertices(File * const input, std::vector<std::pair<unsigned, unsigned>> * const &vertices,
                  std::unordered_map<unsigned, std::string> * const vertex_id_to_vertex_name_map) {
    unsigned line_no(0);
    while (not input->eof()) {
        const auto line(input->getline());
        ++line_no;
        if (unlikely(line.empty()))
            continue;

        const auto arrow_start(line.find("->"));
        if (unlikely(arrow_start == std::string::npos))
            LOG_ERROR("bad input in \"" + input->getPath() + "\" on line #" + std::to_string(line_no) + "! (1)");

        const std::string vertex1(StringUtil::Trim(line.substr(0, arrow_start)));
        const std::string vertex2(StringUtil::Trim(line.substr(arrow_start + 2)));

        vertices->emplace_back(std::make_pair(MapVertexNameToVertexID(vertex1, vertex_id_to_vertex_name_map),
                                              MapVertexNameToVertexID(vertex2, vertex_id_to_vertex_name_map)));
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage(" graph_input_file\nThe lines in the file must have the format \"VertexA -> VertexB\".");

    const std::string input_filename(argv[1]);
    const auto input(FileUtil::OpenInputFileOrDie(input_filename));

    std::vector<std::pair<unsigned, unsigned>> vertices;
    std::unordered_map<unsigned, std::string> vertex_id_to_vertex_name_map;
    LoadVertices(input.get(), &vertices, &vertex_id_to_vertex_name_map);

    std::vector<unsigned> node_order, cycle;
    if (not MiscUtil::TopologicalSort(vertices, &node_order, &cycle)) {
        std::cerr << "Cycle:\n";
        for (const auto &node : cycle)
            std::cerr << '\t' << vertex_id_to_vertex_name_map[node] << '\n';
        return EXIT_FAILURE;
    }

    for (const auto &node : node_order)
        std::cout << vertex_id_to_vertex_name_map[node] << '\n';

    return EXIT_SUCCESS;
}
