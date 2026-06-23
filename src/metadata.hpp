#pragma once
#include <cstdint>
#include <string>
#include <sys/stat.h>
#include "format.hpp"

struct FileMeta {
    std::string path;
    FileType type = FileType::Regular;
    uint16_t permissions = 0644;
    uint32_t uid = 0;
    uint32_t gid = 0;
    uint64_t mtime_ns = 0;
    uint64_t atime_ns = 0;
    std::string symlink_target;
    uint64_t file_size = 0;
    uint64_t inode = 0;
    uint32_t link_count = 0;
};

FileMeta read_metadata(const std::string& path);
void restore_metadata(const std::string& path, const FileMeta& meta);
void ensure_directories(const std::string& path);
