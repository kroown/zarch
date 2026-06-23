#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct MappedFile {
    const uint8_t* data = nullptr;
    size_t size = 0;
    int fd = -1;
    ~MappedFile();
    MappedFile() = default;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    bool map(const std::string& path);
    void unmap();
    bool is_mapped() const { return data != nullptr; }
};

class FileWriter {
public:
    FileWriter();
    ~FileWriter();
    bool open(const std::string& path);
    void write(const uint8_t* data, size_t len);
    void write_at(const uint8_t* data, size_t len, uint64_t offset);
    void close();
    uint64_t tell() const { return m_offset; }
    int fd() const { return m_fd; }

private:
    int m_fd = -1;
    uint64_t m_offset = 0;
};

class FileReader {
public:
    FileReader();
    ~FileReader();
    bool open(const std::string& path);
    size_t read(uint8_t* buf, size_t len);
    void seek(uint64_t pos);
    void close();
    int fd() const { return m_fd; }

private:
    int m_fd = -1;
};
