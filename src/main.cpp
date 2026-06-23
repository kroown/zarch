#include "cli.hpp"
#include "archive.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <cstring>
#include <unistd.h>

int main(int argc, char* argv[]) {
    auto opts = parse_cli(argc, argv);

    if (opts.create) {
        if (!opts.stdin_mode && opts.archive_path.empty()) {
            std::cerr << "zarch: specify --file for output archive\n";
            return 1;
        }

        std::vector<std::string> inputs;

        if (opts.stdin_mode) {
            std::vector<char> buf;
            char ch;
            while (std::fread(&ch, 1, 1, stdin) == 1)
                buf.push_back(ch);
            // stdin: write to temp file then archive
            std::string tmp = "/tmp/zarch_stdin_" + std::to_string(getpid());
            FILE* f = std::fopen(tmp.c_str(), "wb");
            if (f) {
                std::fwrite(buf.data(), 1, buf.size(), f);
                std::fclose(f);
                inputs.push_back(tmp);
            }
            if (opts.archive_path.empty())
                opts.archive_path = "/dev/stdout";
            if (!create_archive(opts.archive_path, inputs,
                                opts.num_threads, opts.verbose, opts.filter))
                return 1;
            std::remove(tmp.c_str());
        } else {
            if (opts.paths.empty()) {
                std::cerr << "zarch: specify input files/directories\n";
                return 1;
            }
            if (!create_archive(opts.archive_path, opts.paths,
                                opts.num_threads, opts.verbose, opts.filter))
                return 1;
        }
    } else if (opts.extract) {
        if (opts.archive_path.empty()) {
            std::cerr << "zarch: specify --file for input archive\n";
            return 1;
        }
        std::string out_dir = ".";
        if (!opts.paths.empty()) out_dir = opts.paths[0];
        if (!extract_archive(opts.archive_path, out_dir, opts.verbose))
            return 1;
    }

    return 0;
}
