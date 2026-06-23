#pragma once
#include <cstdint>
#include <vector>
#include <list>
#include "bitwriter.hpp"
#include "bitreader.hpp"

static constexpr int LZ_MIN_MATCH = 3;
static constexpr int LZ_MAX_MATCH = 258;
static constexpr int LZ_HASH_BITS = 15;
static constexpr int LZ_HASH_SIZE = 1 << LZ_HASH_BITS;
static constexpr int LZ_WINDOW_SIZE = 1 << 20; // 1MB
static constexpr int LZ_MAX_DISTANCE = LZ_WINDOW_SIZE;
static constexpr int LZ_HASH_CHAIN_LEN = 4;

struct Match {
    int distance;
    int length;
};

class LZCompressor {
public:
    LZCompressor();
    void compress(BitWriter& w, const uint8_t* data, size_t len, int level = 3);
    std::vector<uint8_t> compress_block(const uint8_t* data, size_t len, int level = 3);

private:
    std::vector<std::list<int>> m_hash;
    uint32_t hash3(const uint8_t* p) const;
    Match find_match(const uint8_t* data, size_t pos, size_t limit) const;
    void insert(const uint8_t* data, size_t pos);
};

class LZDecompressor {
public:
    std::vector<uint8_t> decompress_block(const uint8_t* data, size_t len, size_t uncomp_size);
    void decompress(BitReader& r, uint8_t* out, size_t count);
};
