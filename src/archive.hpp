#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "format.hpp"
#include "metadata.hpp"

bool create_archive(const std::string& archive_path,
                    const std::vector<std::string>& input_paths,
                    int num_threads,
                    bool verbose,
                    FilterType filter = FilterType::None);

bool extract_archive(const std::string& archive_path,
                     const std::string& output_dir,
                     bool verbose);

// crc32 from zlib
uint32_t crc32_bytes(const uint8_t* data, size_t len);
