#include "bitwriter.hpp"

BitWriter::BitWriter() {
    m_bytes.reserve(65536);
}

void BitWriter::write_bit(int bit) {
    m_buf = (m_buf << 1) | (bit & 1);
    m_bits_in_buf++;
    if (m_bits_in_buf == 64) flush_buffer();
}

void BitWriter::write_bits(uint64_t value, int nbits) {
    for (int i = nbits - 1; i >= 0; i--)
        write_bit((value >> i) & 1);
}

void BitWriter::write_byte(uint8_t byte) {
    write_bits(byte, 8);
}

void BitWriter::flush() {
    if (m_bits_in_buf > 0) {
        m_buf <<= (64 - m_bits_in_buf);
        flush_buffer();
    }
}

void BitWriter::flush_buffer() {
    if (m_bits_in_buf == 0) return;
    int bytes = (m_bits_in_buf + 7) / 8;
    uint64_t net = __builtin_bswap64(m_buf);
    auto ptr = reinterpret_cast<const uint8_t*>(&net);
    m_bytes.insert(m_bytes.end(), ptr, ptr + bytes);
    m_bits_in_buf = 0;
    m_buf = 0;
}

size_t BitWriter::byte_size() const { return m_bytes.size(); }
size_t BitWriter::bit_count() const { return m_bytes.size() * 8 + m_bits_in_buf; }
const uint8_t* BitWriter::data() const { return m_bytes.data(); }

std::vector<uint8_t> BitWriter::consume() {
    flush();
    return std::move(m_bytes);
}
