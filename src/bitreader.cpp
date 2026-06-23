#include "bitreader.hpp"

BitReader::BitReader(const uint8_t* data, size_t size)
    : m_data(data), m_size(size) {}

void BitReader::refill() {
    if (m_byte_pos >= m_size) return;
    int need = 64 - m_bits_in_buf;
    int take = need / 8;
    if (take > static_cast<int>(m_size - m_byte_pos))
        take = static_cast<int>(m_size - m_byte_pos);
    if (take == 0) return;
    uint64_t val = 0;
    std::memcpy(&val, m_data + m_byte_pos, take);
    val = __builtin_bswap64(val) >> (64 - take * 8);
    if (m_bits_in_buf == 0) [[unlikely]]
        m_buf = val;
    else
        m_buf = (m_buf << (take * 8)) | val;
    m_bits_in_buf += take * 8;
    m_byte_pos += take;
}

int BitReader::read_bit() {
    if (m_bits_in_buf == 0) refill();
    if (m_bits_in_buf == 0) return 0;
    m_bits_in_buf--;
    return (m_buf >> m_bits_in_buf) & 1;
}

uint64_t BitReader::read_bits(int nbits) {
    uint64_t val = 0;
    for (int i = 0; i < nbits; i++)
        val = (val << 1) | read_bit();
    return val;
}

uint8_t BitReader::read_byte() {
    return static_cast<uint8_t>(read_bits(8));
}

size_t BitReader::position() const { return m_byte_pos; }
bool BitReader::eof() const { return m_byte_pos >= m_size && m_bits_in_buf == 0; }
