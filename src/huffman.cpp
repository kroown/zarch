#include "huffman.hpp"
#include <algorithm>
#include <cstring>
#include <queue>
#include <vector>

struct NodePtr {
    HuffNode* node;
    bool operator>(const NodePtr& o) const { return node->freq > o.node->freq; }
};

void Huffman::build(const uint64_t frequencies[256]) {
    for (auto& c : m_codes) c = {};
    m_max_bits = 0;

    build_tree(frequencies);
    generate_canonical();
    build_decode_tables();
}

void Huffman::build_tree(const uint64_t frequencies[256]) {
    std::priority_queue<NodePtr, std::vector<NodePtr>, std::greater<NodePtr>> pq;
    std::vector<std::unique_ptr<HuffNode>> nodes;

    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0) {
            auto n = std::make_unique<HuffNode>();
            n->symbol = i;
            n->freq = frequencies[i];
            pq.push({n.get()});
            nodes.push_back(std::move(n));
        }
    }

    if (pq.empty()) {
        auto n = std::make_unique<HuffNode>();
        n->symbol = 0;
        n->freq = 1;
        pq.push({n.get()});
        nodes.push_back(std::move(n));
    }

    if (pq.size() == 1) {
        auto n = std::make_unique<HuffNode>();
        n->freq = 0;
        n->left = pq.top().node;
        pq.pop();
        pq.push({n.get()});
        nodes.push_back(std::move(n));
    }

    while (pq.size() > 1) {
        auto a = pq.top(); pq.pop();
        auto b = pq.top(); pq.pop();
        auto n = std::make_unique<HuffNode>();
        n->freq = a.node->freq + b.node->freq;
        n->left = a.node;
        n->right = b.node;
        pq.push({n.get()});
        nodes.push_back(std::move(n));
    }

    auto root = pq.top().node;
    assign_codes(root, 0, 0);
}

void Huffman::assign_codes(HuffNode* node, uint64_t code, int len) {
    if (!node->left && !node->right) {
        if (node->symbol >= 0 && node->symbol < 256) {
            m_codes[node->symbol].code = code;
            m_codes[node->symbol].len = len;
            m_bitlens[node->symbol] = len;
            if (len > m_max_bits) m_max_bits = len;
        }
        return;
    }
    if (node->left) assign_codes(node->left, (code << 1), len + 1);
    if (node->right) assign_codes(node->right, (code << 1) | 1, len + 1);
}

void Huffman::generate_canonical() {
    std::vector<std::pair<int, int>> syms;
    for (int i = 0; i < 256; i++)
        if (m_bitlens[i] > 0)
            syms.push_back({m_bitlens[i], i});
    std::sort(syms.begin(), syms.end(),
        [](auto& a, auto& b) {
            if (a.first != b.first) return a.first < b.first;
            return a.second < b.second;
        });

    uint64_t code = 0;
    int prev_len = 0;
    for (auto& [len, sym] : syms) {
        code <<= (len - prev_len);
        m_codes[sym].code = code;
        m_codes[sym].len = len;
        code++;
        prev_len = len;
    }
}

void Huffman::build_decode_tables() {
    std::fill(m_first_code.begin(), m_first_code.end(), 0);
    std::fill(m_sym_count.begin(), m_sym_count.end(), 0);
    for (auto& arr : m_decode_syms) arr.fill(0);

    std::vector<std::pair<int, int>> syms;
    for (int i = 0; i < 256; i++)
        if (m_bitlens[i] > 0)
            syms.push_back({m_bitlens[i], i});
    std::sort(syms.begin(), syms.end());

    uint64_t code = 0;
    int prev_len = 0;
    for (size_t i = 0; i < syms.size(); ) {
        int len = syms[i].first;
        code <<= (len - prev_len);
        m_first_code[len] = code;
        int count = 0;
        while (i < syms.size() && syms[i].first == len) {
            m_decode_syms[len][count] = static_cast<uint8_t>(syms[i].second);
            count++;
            i++;
        }
        m_sym_count[len] = static_cast<uint16_t>(count);
        code += count;
        prev_len = len;
    }
}

void Huffman::serialize_table(BitWriter& w) const {
    int used = 0;
    for (int i = 0; i < 256; i++)
        if (m_bitlens[i] > 0) used++;
    w.write_bits(used, 9);
    int max_bl = 0;
    for (int i = 0; i < 256; i++)
        if (m_bitlens[i] > max_bl) max_bl = m_bitlens[i];
    w.write_bits(max_bl, 5);
    for (int i = 0; i < 256; i++) {
        if (m_bitlens[i] > 0) {
            w.write_byte(i);
            w.write_bits(m_bitlens[i], 5);
        }
    }
}

void Huffman::deserialize_table(BitReader& r) {
    int bitlens[256] = {};
    int used = r.read_bits(9);
    r.read_bits(5);
    for (int i = 0; i < used; i++) {
        int sym = r.read_byte();
        int bl = r.read_bits(5);
        bitlens[sym] = bl;
    }
    build_from_bitlens(bitlens);
}

void Huffman::build_from_bitlens(const int bitlens[256]) {
    std::memcpy(m_bitlens.data(), bitlens, 256 * sizeof(int));
    for (auto& c : m_codes) c = {};
    m_max_bits = 0;
    for (int i = 0; i < 256; i++)
        if (bitlens[i] > m_max_bits) m_max_bits = bitlens[i];
    generate_canonical();
    build_decode_tables();
}

void Huffman::encode(BitWriter& w, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        auto& c = m_codes[data[i]];
        w.write_bits(c.code, c.len);
    }
}

std::vector<uint8_t> Huffman::decode(BitReader& r, size_t count) {
    std::vector<uint8_t> out;
    out.reserve(count);

    for (size_t i = 0; i < count && !r.eof(); i++) {
        uint64_t code = 0;
        int found = -1;
        for (int len = 1; len <= m_max_bits; len++) {
            if (r.eof()) break;
            code = (code << 1) | r.read_bit();
            if (m_sym_count[len] > 0) {
                uint64_t first = m_first_code[len];
                if (code >= first && code < first + m_sym_count[len]) {
                    int idx = static_cast<int>(code - first);
                    found = m_decode_syms[len][idx];
                    break;
                }
            }
        }
        if (found < 0) found = 0;
        out.push_back(static_cast<uint8_t>(found));
    }
    return out;
}

std::vector<uint8_t> Huffman::decode_all(BitReader& r) {
    std::vector<uint8_t> out;
    while (!r.eof()) {
        uint64_t code = 0;
        int found = -1;
        for (int len = 1; len <= m_max_bits; len++) {
            if (r.eof()) break;
            code = (code << 1) | r.read_bit();
            if (m_sym_count[len] > 0) {
                uint64_t first = m_first_code[len];
                if (code >= first && code < first + m_sym_count[len]) {
                    int idx = static_cast<int>(code - first);
                    found = m_decode_syms[len][idx];
                    break;
                }
            }
        }
        if (found < 0) found = 0;
        out.push_back(static_cast<uint8_t>(found));
    }
    return out;
}
