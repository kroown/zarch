#include "cli.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <cstdlib>
#include <unistd.h>

CLIOptions parse_cli(int argc, char* argv[]) {
    CLIOptions opts;
    opts.num_threads = static_cast<int>(std::thread::hardware_concurrency());

    if (argc < 2) { print_usage(); std::exit(1); }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-c" || arg == "--create") opts.create = true;
        else if (arg == "-x" || arg == "--extract") opts.extract = true;
        else if (arg == "-f" || arg == "--file") {
            if (++i >= argc) {
                std::cerr << "zarch: --file requires a path\n";
                std::exit(1);
            }
            opts.archive_path = argv[i];
        }
        else if (arg == "-v" || arg == "--verbose") opts.verbose = true;
        else if (arg == "-t" || arg == "--threads") {
            if (++i >= argc) {
                std::cerr << "zarch: --threads requires a number\n";
                std::exit(1);
            }
            opts.num_threads = std::max(1, std::atoi(argv[i]));
        }
        else if (arg == "--filter") {
            if (++i >= argc) {
                std::cerr << "zarch: --filter requires an argument\n";
                std::exit(1);
            }
            std::string f = argv[i];
            if (f == "bcj") opts.filter = FilterType::BCJ;
            else if (f == "delta") opts.filter = FilterType::Delta;
        }
        else if (arg == "--help" || arg == "-h") { print_usage(); std::exit(0); }
        else {
            opts.paths.push_back(arg);
        }
    }

    // Stdin mode: if no paths and create
    if (opts.create && opts.paths.empty() && !isatty(STDIN_FILENO))
        opts.stdin_mode = true;

    if (!opts.create && !opts.extract) {
        std::cerr << "zarch: specify --create (-c) or --extract (-x)\n";
        std::exit(1);
    }

    return opts;
}

void print_usage() {
    std::cout << "zarch v1.0 - Production-Grade Linux File Archiver\n"
              << "Usage:\n"
              << "  zarch -c -f archive.zarch [files...]   Create archive\n"
              << "  zarch -x -f archive.zarch              Extract archive\n"
              << "  cat data | zarch -c > out.zarch        Stdin pipe mode\n"
              << "\n"
              << "Options:\n"
              << "  -c, --create          Create archive\n"
              << "  -x, --extract         Extract archive\n"
              << "  -f, --file <path>     Target .zarch file\n"
              << "  -v, --verbose         Verbose output\n"
              << "  -t, --threads <num>   Worker threads (default: all cores)\n"
              << "  --filter <bcj|delta>  Pre-processing filter\n"
              << "  --help                Show this help\n";
}
