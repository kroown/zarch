#include "metadata.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <libgen.h>

FileMeta read_metadata(const std::string& path) {
    struct stat st;
    FileMeta meta;
    meta.path = path;

    if (lstat(path.c_str(), &st) < 0) {
        std::cerr << "zarch: cannot stat '" << path << "': " << strerror(errno) << "\n";
        return meta;
    }

    if (S_ISREG(st.st_mode)) meta.type = FileType::Regular;
    else if (S_ISLNK(st.st_mode)) meta.type = FileType::Symlink;
    else if (S_ISDIR(st.st_mode)) { meta.type = FileType::Regular; return meta; }
    else return meta;

    meta.permissions = st.st_mode & 07777;
    meta.uid = st.st_uid;
    meta.gid = st.st_gid;
    meta.mtime_ns = static_cast<uint64_t>(st.st_mtim.tv_sec) * 1000000000 +
                    static_cast<uint64_t>(st.st_mtim.tv_nsec);
    meta.atime_ns = static_cast<uint64_t>(st.st_atim.tv_sec) * 1000000000 +
                    static_cast<uint64_t>(st.st_atim.tv_nsec);
    meta.file_size = static_cast<uint64_t>(st.st_size);
    meta.inode = static_cast<uint64_t>(st.st_ino);
    meta.link_count = static_cast<uint32_t>(st.st_nlink);

    if (meta.type == FileType::Symlink) {
        std::vector<char> buf(4096);
        ssize_t len = readlink(path.c_str(), buf.data(), buf.size() - 1);
        if (len >= 0) {
            buf[len] = 0;
            meta.symlink_target = buf.data();
        }
    }

    return meta;
}

void restore_metadata(const std::string& path, const FileMeta& meta) {
    if (meta.type == FileType::Symlink) {
        symlink(meta.symlink_target.c_str(), path.c_str());
    }

    chmod(path.c_str(), meta.permissions);
    chown(path.c_str(), meta.uid, meta.gid);

    struct timespec times[2];
    times[0].tv_sec = meta.atime_ns / 1000000000;
    times[0].tv_nsec = meta.atime_ns % 1000000000;
    times[1].tv_sec = meta.mtime_ns / 1000000000;
    times[1].tv_nsec = meta.mtime_ns % 1000000000;
    utimensat(AT_FDCWD, path.c_str(), times, AT_SYMLINK_NOFOLLOW);
}

void ensure_directories(const std::string& path) {
    auto d = path;
    for (size_t i = 1; i < d.size(); i++) {
        if (d[i] == '/') {
            d[i] = 0;
            mkdir(d.c_str(), 0755);
            d[i] = '/';
        }
    }
}
