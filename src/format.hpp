#pragma once
#include <cstdint>
#include <string>
#include <vector>

static constexpr const char* ZARCH_MAGIC = "ZARC";
static constexpr uint16_t ZARCH_VERSION = 0x0001;
static constexpr uint16_t ZARCH_OS_LINUX = 0x0001;
static constexpr size_t ZARCH_HEADER_SIZE = 32;
static constexpr size_t ZARCH_BLOCK_PREFIX_SIZE = 12;
static constexpr size_t ZARCH_DEFAULT_CHUNK = 4 * 1024 * 1024;

#pragma pack(push, 1)
struct ArchiveHeader {
    char     magic[4];        // "zarc"
    uint16_t version;         // 0x0001
    uint16_t target_os;       // 0x0001 = linux
    uint64_t index_offset;    // offset to footer
    uint64_t block_count;
    uint8_t  filter_type;     // 0=none, 1=bcj, 2=delta
    uint8_t  reserved[7];
};

struct BlockPrefix {
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t checksum;        // crc32 of compressed data
};

enum class FileType : uint8_t {
    Regular  = 0x01,
    Symlink  = 0x02,
    Hardlink = 0x03,
};

struct FileEntry {
    uint16_t path_len;
    std::string path;
    FileType file_type;
    uint16_t permissions;     // e.g., 0755
    uint32_t uid;
    uint32_t gid;
    uint64_t mtime_ns;
    uint64_t atime_ns;
    uint16_t symlink_len;
    std::string symlink_target;
    uint32_t data_block_id;
    uint64_t block_offset;
    uint64_t file_size;
};
#pragma pack(pop)

enum class FilterType : uint8_t {
    None  = 0x00,
    BCJ   = 0x01,
    Delta = 0x02,
};
