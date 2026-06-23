#include "archive.hpp"
#include "io.hpp"
#include "lz.hpp"
#include "huffman.hpp"
#include "filters.hpp"
#include "thread_pool.hpp"
#include "metadata.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <zlib.h>

namespace fs = std::filesystem;

static std::vector<uint8_t> compress_block_data(const std::vector<uint8_t>& input) {
    LZCompressor lz;
    auto lz_out = lz.compress_block(input.data(), input.size());

    uint64_t freqs[256] = {};
    for (auto b : lz_out) freqs[b]++;

    Huffman huff;
    huff.build(freqs);

    BitWriter w;
    huff.serialize_table(w);
    huff.encode(w, lz_out.data(), lz_out.size());
    return w.consume();
}

static std::vector<uint8_t> decompress_block_data(const uint8_t* data, size_t len,
                                                    size_t uncomp_size) {
    BitReader r(data, len);
    Huffman huff;
    huff.deserialize_table(r);

    auto lz_compressed = huff.decode_all(r);

    if (lz_compressed.empty()) {
        return std::vector<uint8_t>(uncomp_size, 0);
    }

    LZDecompressor lz;
    return lz.decompress_block(lz_compressed.data(), lz_compressed.size(), uncomp_size);
}

bool create_archive(const std::string& archive_path,
                    const std::vector<std::string>& input_paths,
                    int num_threads,
                    bool verbose,
                    FilterType filter) {
    std::vector<std::string> file_list;
    for (const auto& path : input_paths) {
        if (fs::is_directory(path)) {
            for (auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file() || entry.is_symlink())
                    file_list.push_back(entry.path().string());
            }
        } else if (fs::is_regular_file(path) || fs::is_symlink(path)) {
            file_list.push_back(path);
        }
    }

    if (file_list.empty()) {
        std::cerr << "zarch: no input files\n";
        return false;
    }

    std::vector<FileMeta> metas;
    std::vector<MappedFile> mapped;
    std::vector<std::vector<uint8_t>> filtered_data;
    size_t total_size = 0;

    for (auto& f : file_list) {
        auto meta = read_metadata(f);
        if (meta.type == FileType::Regular) {
            MappedFile mf;
            if (mf.map(f)) {
                total_size += mf.size;
                mapped.push_back(std::move(mf));
                metas.push_back(meta);
                filtered_data.emplace_back();
            }
        } else if (meta.type == FileType::Symlink) {
            mapped.push_back({});
            metas.push_back(meta);
            filtered_data.emplace_back();
        }
    }

    struct DataChunk {
        int block_id;
        size_t file_idx;
        size_t file_offset;
        size_t size;
        const uint8_t* data;
    };
    std::vector<DataChunk> chunks;
    int block_id = 0;

    for (size_t i = 0; i < metas.size(); i++) {
        if (metas[i].type != FileType::Regular) continue;
        if (!mapped[i].is_mapped()) continue;

        const uint8_t* file_ptr = mapped[i].data;
        size_t file_size = mapped[i].size;

        if (filter != FilterType::None) {
            filtered_data[i].assign(mapped[i].data, mapped[i].data + mapped[i].size);
            if (filter == FilterType::BCJ) apply_bcj_filter(filtered_data[i]);
            else if (filter == FilterType::Delta) apply_delta_filter(filtered_data[i]);
            file_ptr = filtered_data[i].data();
            file_size = filtered_data[i].size();
        }

        size_t offset = 0;
        while (offset < file_size) {
            size_t chunk_size = std::min(ZARCH_DEFAULT_CHUNK, file_size - offset);
            chunks.push_back({block_id++, i, offset,
                              chunk_size,
                              file_ptr + offset});
            offset += chunk_size;
        }
    }

    int num_blocks = block_id;
    if (verbose)
        std::cout << "zarch: " << num_blocks << " data blocks, "
                  << metas.size() << " files\n";

    ThreadPool pool(num_threads);
    std::vector<ThreadPool::BlockTask> results(num_blocks);

    for (auto& ch : chunks) {
        std::vector<uint8_t> inp(ch.data, ch.data + ch.size);
        pool.enqueue(ch.block_id, std::move(inp),
            [](ThreadPool::BlockTask& task) {
                task.output = compress_block_data(task.input);
                task.checksum = crc32_bytes(task.output.data(), task.output.size());
            });
    }

    FileWriter writer;
    if (!writer.open(archive_path)) return false;

    ArchiveHeader hdr = {};
    std::memcpy(hdr.magic, ZARCH_MAGIC, 4);
    hdr.version = ZARCH_VERSION;
    hdr.target_os = ZARCH_OS_LINUX;
    hdr.block_count = num_blocks;
    hdr.filter_type = static_cast<uint8_t>(filter);
    writer.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr));

    std::vector<uint64_t> block_positions(num_blocks);
    std::vector<uint64_t> uncomp_sizes(num_blocks);

    for (int i = 0; i < num_blocks; i++) {
        ThreadPool::BlockTask task;
        if (!pool.wait_for_result(task)) {
            std::cerr << "zarch: compression failed\n";
            return false;
        }
        results[task.block_id] = std::move(task);
    }

    for (int i = 0; i < num_blocks; i++) {
        block_positions[i] = writer.tell();
        auto& r = results[i];
        uncomp_sizes[i] = r.uncompressed_size;

        BlockPrefix prefix;
        prefix.compressed_size = static_cast<uint32_t>(r.output.size());
        prefix.uncompressed_size = static_cast<uint32_t>(r.uncompressed_size);
        prefix.checksum = r.checksum;

        writer.write(reinterpret_cast<const uint8_t*>(&prefix), sizeof(prefix));
        writer.write(r.output.data(), r.output.size());
    }

    if (verbose)
        std::cout << "zarch: compressed " << total_size << " bytes -> "
                  << writer.tell() - ZARCH_HEADER_SIZE << " bytes ("
                  << (writer.tell() - ZARCH_HEADER_SIZE) * 100 / std::max(total_size, size_t(1))
                  << "%)\n";

    uint64_t footer_offset = writer.tell();

    uint32_t entry_count = 0;
    for (auto& m : metas) {
        if (m.type == FileType::Regular || m.type == FileType::Symlink)
            entry_count++;
    }
    writer.write(reinterpret_cast<const uint8_t*>(&entry_count), 4);

    int current_block = 0;
    uint64_t block_offset = 0;

    for (size_t i = 0; i < metas.size(); i++) {
        auto& meta = metas[i];
        if (meta.type != FileType::Regular && meta.type != FileType::Symlink)
            continue;

        FileEntry entry = {};

        std::string rel_path = meta.path;
        if (input_paths.size() == 1 && fs::is_directory(input_paths[0])) {
            rel_path = fs::relative(meta.path, input_paths[0]).string();
        }

        entry.path_len = static_cast<uint16_t>(rel_path.size());
        entry.file_type = meta.type;
        entry.permissions = meta.permissions;
        entry.uid = meta.uid;
        entry.gid = meta.gid;
        entry.mtime_ns = meta.mtime_ns;
        entry.atime_ns = meta.atime_ns;
        entry.file_size = meta.file_size;

        if (meta.type == FileType::Symlink) {
            entry.symlink_len = static_cast<uint16_t>(meta.symlink_target.size());
        } else {
            entry.data_block_id = current_block;
            entry.block_offset = block_offset;
            entry.symlink_len = 0;

            if (current_block < num_blocks) {
                block_offset += uncomp_sizes[current_block];
                if (block_offset >= (mapped[i].is_mapped() ? mapped[i].size : meta.file_size)) {
                    current_block++;
                    block_offset = 0;
                }
            }
        }

        writer.write(reinterpret_cast<const uint8_t*>(&entry.path_len), 2);
        writer.write(reinterpret_cast<const uint8_t*>(rel_path.data()), rel_path.size());
        writer.write(reinterpret_cast<const uint8_t*>(&entry.file_type), 1);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.permissions), 2);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.uid), 4);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.gid), 4);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.mtime_ns), 8);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.atime_ns), 8);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.symlink_len), 2);
        if (entry.symlink_len > 0) {
            writer.write(reinterpret_cast<const uint8_t*>(meta.symlink_target.data()),
                         meta.symlink_target.size());
        }
        writer.write(reinterpret_cast<const uint8_t*>(&entry.data_block_id), 4);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.block_offset), 8);
        writer.write(reinterpret_cast<const uint8_t*>(&entry.file_size), 8);
    }

    hdr.index_offset = footer_offset;
    writer.write_at(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr), 0);

    writer.close();

    if (verbose)
        std::cout << "zarch: wrote " << writer.tell() << " bytes total to '"
                  << archive_path << "'\n";
    return true;
}

bool extract_archive(const std::string& archive_path,
                     const std::string& output_dir,
                     bool verbose) {
    MappedFile mf;
    if (!mf.map(archive_path)) return false;
    if (mf.size < ZARCH_HEADER_SIZE) {
        std::cerr << "zarch: invalid archive (too small)\n";
        return false;
    }

    const auto* hdr = reinterpret_cast<const ArchiveHeader*>(mf.data);
    if (std::memcmp(hdr->magic, ZARCH_MAGIC, 4) != 0) {
        std::cerr << "zarch: invalid magic bytes\n";
        return false;
    }

    uint64_t footer_offset = hdr->index_offset;
    uint64_t block_count = hdr->block_count;
    FilterType archive_filter = static_cast<FilterType>(hdr->filter_type);

    std::vector<std::vector<uint8_t>> compressed_blocks(block_count);
    std::vector<size_t> uncomp_sizes(block_count);

    size_t data_pos = sizeof(ArchiveHeader);
    for (uint64_t i = 0; i < block_count; i++) {
        if (data_pos + sizeof(BlockPrefix) > mf.size) break;
        auto* bp = reinterpret_cast<const BlockPrefix*>(mf.data + data_pos);
        data_pos += sizeof(BlockPrefix);

        if (data_pos + bp->compressed_size > mf.size) break;
        compressed_blocks[i].assign(mf.data + data_pos,
                                     mf.data + data_pos + bp->compressed_size);
        uncomp_sizes[i] = bp->uncompressed_size;
        data_pos += bp->compressed_size;
    }

    std::vector<std::vector<uint8_t>> decompressed_blocks(block_count);

    ThreadPool pool(std::thread::hardware_concurrency());
    for (size_t i = 0; i < block_count; i++) {
        std::vector<uint8_t> inp = compressed_blocks[i];
        size_t uncomp = uncomp_sizes[i];
        pool.enqueue(static_cast<int>(i), std::move(inp),
            [uncomp](ThreadPool::BlockTask& task) {
                task.output = decompress_block_data(
                    task.input.data(), task.input.size(), uncomp);
            });
    }

    for (size_t i = 0; i < block_count; i++) {
        ThreadPool::BlockTask task;
        pool.wait_for_result(task);
        decompressed_blocks[task.block_id] = std::move(task.output);
    }

    if (footer_offset >= mf.size) {
        std::cerr << "zarch: invalid footer offset\n";
        return false;
    }

    size_t fpos = static_cast<size_t>(footer_offset);
    uint32_t entry_count;
    std::memcpy(&entry_count, mf.data + fpos, 4);
    fpos += 4;

    fs::create_directories(output_dir);

    for (uint32_t i = 0; i < entry_count; i++) {
        FileEntry entry = {};

        if (fpos + 2 > mf.size) break;
        std::memcpy(&entry.path_len, mf.data + fpos, 2); fpos += 2;

        if (fpos + entry.path_len > mf.size) break;
        entry.path.assign(reinterpret_cast<const char*>(mf.data + fpos), entry.path_len);
        fpos += entry.path_len;

        if (fpos + 1 > mf.size) break;
        std::memcpy(&entry.file_type, mf.data + fpos, 1); fpos += 1;
        std::memcpy(&entry.permissions, mf.data + fpos, 2); fpos += 2;
        std::memcpy(&entry.uid, mf.data + fpos, 4); fpos += 4;
        std::memcpy(&entry.gid, mf.data + fpos, 4); fpos += 4;
        std::memcpy(&entry.mtime_ns, mf.data + fpos, 8); fpos += 8;
        std::memcpy(&entry.atime_ns, mf.data + fpos, 8); fpos += 8;
        std::memcpy(&entry.symlink_len, mf.data + fpos, 2); fpos += 2;

        if (entry.symlink_len > 0) {
            if (fpos + entry.symlink_len > mf.size) break;
            entry.symlink_target.assign(reinterpret_cast<const char*>(mf.data + fpos),
                                        entry.symlink_len);
            fpos += entry.symlink_len;
        }

        std::memcpy(&entry.data_block_id, mf.data + fpos, 4); fpos += 4;
        std::memcpy(&entry.block_offset, mf.data + fpos, 8); fpos += 8;
        std::memcpy(&entry.file_size, mf.data + fpos, 8); fpos += 8;

        std::string out_path = output_dir + "/" + entry.path;
        ensure_directories(out_path);

        FileMeta meta;
        meta.path = out_path;
        meta.type = entry.file_type;
        meta.permissions = entry.permissions;
        meta.uid = entry.uid;
        meta.gid = entry.gid;
        meta.mtime_ns = entry.mtime_ns;
        meta.atime_ns = entry.atime_ns;
        meta.symlink_target = entry.symlink_target;
        meta.file_size = entry.file_size;

        if (entry.file_type == FileType::Regular) {
            size_t block_id = entry.data_block_id;
            size_t offset = static_cast<size_t>(entry.block_offset);
            size_t fsize = static_cast<size_t>(entry.file_size);

            std::vector<uint8_t> file_data;
            file_data.reserve(fsize);

            size_t remaining = fsize;
            while (remaining > 0 && block_id < decompressed_blocks.size()) {
                auto& block = decompressed_blocks[block_id];
                size_t avail = block.size() - offset;
                size_t to_copy = std::min(remaining, avail);
                file_data.insert(file_data.end(),
                                 block.data() + offset,
                                 block.data() + offset + to_copy);
                remaining -= to_copy;
                block_id++;
                offset = 0;
            }

            if (archive_filter == FilterType::BCJ) inverse_bcj_filter(file_data);
            else if (archive_filter == FilterType::Delta) inverse_delta_filter(file_data);

            FileWriter fw;
            if (fw.open(out_path)) {
                fw.write(file_data.data(), file_data.size());
                fw.close();
            }
        } else if (entry.file_type == FileType::Symlink) {
            fs::create_symlink(entry.symlink_target, out_path);
        }

        restore_metadata(out_path, meta);

        if (verbose)
            std::cout << "  " << entry.path << " (" << entry.file_size << " bytes)\n";
    }

    if (verbose)
        std::cout << "zarch: extracted " << entry_count << " files to '" << output_dir << "'\n";
    return true;
}

uint32_t crc32_bytes(const uint8_t* data, size_t len) {
    return crc32(0, data, static_cast<uInt>(len));
}
