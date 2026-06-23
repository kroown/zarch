#include "io.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <algorithm>

MappedFile::~MappedFile() { unmap(); }

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data(other.data), size(other.size), fd(other.fd) {
    other.data = nullptr;
    other.size = 0;
    other.fd = -1;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        unmap();
        data = other.data;
        size = other.size;
        fd = other.fd;
        other.data = nullptr;
        other.size = 0;
        other.fd = -1;
    }
    return *this;
}

bool MappedFile::map(const std::string& path) {
    fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "zarch: cannot open '" << path << "': " << strerror(errno) << "\n";
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) { ::close(fd); fd = -1; return false; }
    size = st.st_size;
    if (size == 0) return true;
    data = static_cast<const uint8_t*>(
        mmap(nullptr, size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0));
    if (data == MAP_FAILED) {
        std::cerr << "zarch: mmap failed: " << strerror(errno) << "\n";
        ::close(fd); fd = -1; data = nullptr; return false;
    }
    madvise(const_cast<uint8_t*>(data), size, MADV_SEQUENTIAL | MADV_WILLNEED);
    return true;
}

void MappedFile::unmap() {
    if (data) { munmap(const_cast<uint8_t*>(data), size); data = nullptr; }
    if (fd >= 0) { ::close(fd); fd = -1; }
    size = 0;
}

FileWriter::FileWriter() {}
FileWriter::~FileWriter() { if (m_fd >= 0) close(); }

bool FileWriter::open(const std::string& path) {
    m_fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (m_fd < 0) {
        std::cerr << "zarch: cannot create '" << path << "': " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

void FileWriter::write(const uint8_t* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(m_fd, data + written, len - written);
        if (n <= 0) break;
        written += n;
    }
    m_offset += written;
}

void FileWriter::write_at(const uint8_t* data, size_t len, uint64_t offset) {
    ::pwrite(m_fd, data, len, offset);
}

void FileWriter::close() {
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
}

FileReader::FileReader() {}
FileReader::~FileReader() { if (m_fd >= 0) close(); }

bool FileReader::open(const std::string& path) {
    m_fd = ::open(path.c_str(), O_RDONLY);
    if (m_fd < 0) {
        std::cerr << "zarch: cannot open '" << path << "': " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

size_t FileReader::read(uint8_t* buf, size_t len) {
    ssize_t n = ::read(m_fd, buf, len);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

void FileReader::seek(uint64_t pos) {
    ::lseek(m_fd, static_cast<off_t>(pos), SEEK_SET);
}

void FileReader::close() {
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
}
