/** \brief A test harness for MiscUtil::TopologicalSort. */
#include <iostream>
#include <unordered_set>
#include <cstdlib>
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " edge1 edge2 ... edgeN\n";
    std::exit(EXIT_FAILURE);
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 3 or (argc % 2) != 1)
        Usage();

    std::unordered_set<unsigned> vertices;
    std::vector<std::pair<unsigned, unsigned>> edges;
    unsigned first_vertex;
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        unsigned vertex;
        if (not StringUtil::ToUnsigned(argv[arg_no], &vertex))
            logger->error("bad vertex: " + std::string(argv[arg_no]));
        vertices.emplace(vertex);
        if ((arg_no % 2) == 1)
            first_vertex = vertex;
        else
            edges.emplace_back(first_vertex, vertex);
    }
    std::cout << "Read " << edges.size() << " edges.\n";

    std::vector<unsigned> sorted_vertices;
    if (not MiscUtil::TopologicalSort(edges, &sorted_vertices))
        logger->error("we have a cycle!");
    for (auto v : sorted_vertices)
        std::cout << v << '\n';
}
