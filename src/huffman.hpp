#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include "bitwriter.hpp"
#include "bitreader.hpp"

static constexpr int HUFF_SYMBOLS = 256;

struct HuffNode {
    int symbol = -1;
    uint64_t freq = 0;
    HuffNode* left = nullptr;
    HuffNode* right = nullptr;
};

struct HuffCode {
    uint64_t code = 0;
    int len = 0;
};

class Huffman {
public:
    void build(const uint64_t frequencies[256]);
    void encode(BitWriter& w, const uint8_t* data, size_t len);
    std::vector<uint8_t> decode(BitReader& r, size_t count);
    std::vector<uint8_t> decode_all(BitReader& r);

    void serialize_table(BitWriter& w) const;
    void deserialize_table(BitReader& r);

    const HuffCode* codes() const { return m_codes; }

private:
    HuffCode m_codes[256]{};
    std::array<int, 256> m_bitlens{};
    int m_max_bits = 0;

    // canonical decode helpers, size 32 = supports 5-bit bitlen 0..31
    std::array<uint64_t, 32> m_first_code{};
    std::array<uint16_t, 32> m_sym_count{};
    std::array<std::array<uint8_t, 256>, 32> m_decode_syms{};

    void build_tree(const uint64_t frequencies[256]);
    void assign_codes(HuffNode* node, uint64_t code, int len);
    void generate_canonical();
    void build_from_bitlens(const int bitlens[256]);
    void build_decode_tables();
};
