#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

class BitWriter {
public:
    BitWriter();
    void write_bit(int bit);
    void write_bits(uint64_t value, int nbits);
    void write_byte(uint8_t byte);
    void flush();
    size_t byte_size() const;
    size_t bit_count() const;
    const uint8_t* data() const;
    std::vector<uint8_t> consume();

private:
    uint64_t m_buf = 0;
    int m_bits_in_buf = 0;
    std::vector<uint8_t> m_bytes;
    void flush_buffer();
};
