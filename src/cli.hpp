#pragma once
#include <string>
#include <vector>
#include "format.hpp"

struct CLIOptions {
    bool create = false;
    bool extract = false;
    std::string archive_path;
    bool verbose = false;
    int num_threads = 0;
    FilterType filter = FilterType::None;
    std::vector<std::string> paths;
    bool stdin_mode = false;
};

CLIOptions parse_cli(int argc, char* argv[]);
void print_usage();
