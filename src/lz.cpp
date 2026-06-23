#include "lz.hpp"
#include "bitwriter.hpp"
#include "bitreader.hpp"
#include <cstring>
#include <algorithm>

LZCompressor::LZCompressor() : m_hash(LZ_HASH_SIZE) {}

uint32_t LZCompressor::hash3(const uint8_t* p) const {
    return (static_cast<uint32_t>(p[0]) << 12) ^
           (static_cast<uint32_t>(p[1]) << 6) ^
           static_cast<uint32_t>(p[2]);
}

void LZCompressor::insert(const uint8_t* data, size_t pos) {
    auto h = hash3(data + pos) & (LZ_HASH_SIZE - 1);
    auto& chain = m_hash[h];
    chain.push_front(static_cast<int>(pos));
    if (chain.size() > LZ_HASH_CHAIN_LEN)
        chain.pop_back();
}

Match LZCompressor::find_match(const uint8_t* data, size_t pos, size_t limit) const {
    Match best = {0, 0};
    auto h = hash3(data + pos) & (LZ_HASH_SIZE - 1);
    const auto& chain = m_hash[h];
    int max_dist = std::min(static_cast<int>(pos), LZ_MAX_DISTANCE);
    size_t max_len = std::min(limit - pos, static_cast<size_t>(LZ_MAX_MATCH));

    for (auto idx : chain) {
        int dist = static_cast<int>(pos) - idx;
        if (dist <= 0 || dist > max_dist) continue;

        size_t match_len = 0;
        while (match_len < max_len &&
               data[idx + match_len] == data[pos + match_len])
            match_len++;

        if (match_len >= LZ_MIN_MATCH && match_len > static_cast<size_t>(best.length)) {
            best.distance = dist;
            best.length = static_cast<int>(match_len);
            if (match_len == max_len) break;
        }
    }
    return best;
}

void LZCompressor::compress(BitWriter& w, const uint8_t* data, size_t len, int /*level*/) {
    size_t pos = 0;
    for (auto& l : m_hash) l.clear();

    while (pos < len) {
        size_t remain = len - pos;
        if (remain < LZ_MIN_MATCH || pos < LZ_MIN_MATCH - 1) {
            w.write_bit(0);
            w.write_byte(data[pos]);
            pos++;
            continue;
        }

        Match m = find_match(data, pos, len);
        if (m.length >= LZ_MIN_MATCH) {
            w.write_bit(1);
            w.write_bits(m.distance - 1, 20);
            w.write_bits(m.length - LZ_MIN_MATCH, 8);
            for (int i = 0; i < m.length; i++)
                insert(data, pos + i);
            pos += m.length;
        } else {
            w.write_bit(0);
            w.write_byte(data[pos]);
            insert(data, pos);
            pos++;
        }
    }
}

std::vector<uint8_t> LZCompressor::compress_block(const uint8_t* data, size_t len, int level) {
    BitWriter w;
    compress(w, data, len, level);
    return w.consume();
}

void LZDecompressor::decompress(BitReader& r, uint8_t* out, size_t count) {
    std::vector<uint8_t> window;
    window.reserve(LZ_WINDOW_SIZE * 2);
    size_t written = 0;

    while (written < count) {
        int is_match = r.read_bit();
        if (is_match) {
            int dist = static_cast<int>(r.read_bits(20)) + 1;
            int len = static_cast<int>(r.read_bits(8)) + LZ_MIN_MATCH;
            if (len > static_cast<int>(count - written))
                len = static_cast<int>(count - written);
            for (int i = 0; i < len && written < count; i++) {
                int src_idx = static_cast<int>(window.size()) - dist;
                uint8_t b = (src_idx >= 0) ? window[src_idx] : 0;
                window.push_back(b);
                out[written++] = b;
            }
        } else {
            uint8_t b = r.read_byte();
            window.push_back(b);
            out[written++] = b;
        }
    }
}

std::vector<uint8_t> LZDecompressor::decompress_block(const uint8_t* data, size_t len, size_t uncomp_size) {
    BitReader r(data, len);
    std::vector<uint8_t> out(uncomp_size);
    decompress(r, out.data(), uncomp_size);
    return out;
}
