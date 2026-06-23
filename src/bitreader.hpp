#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size);
    int read_bit();
    uint64_t read_bits(int nbits);
    uint8_t read_byte();
    size_t position() const;
    bool eof() const;

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_byte_pos = 0;
    uint64_t m_buf = 0;
    int m_bits_in_buf = 0;
    void refill();
};
