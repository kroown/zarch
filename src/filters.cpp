#include "filters.hpp"

void apply_bcj_filter(std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 4 < data.size(); i++) {
        if (data[i] == 0xE8 || data[i] == 0xE9) {
            int32_t rel = static_cast<int32_t>(
                static_cast<uint32_t>(data[i+1]) |
                (static_cast<uint32_t>(data[i+2]) << 8) |
                (static_cast<uint32_t>(data[i+3]) << 16) |
                (static_cast<uint32_t>(data[i+4]) << 24));
            int32_t abs = rel + static_cast<int32_t>(i) + 5;
            data[i+1] = static_cast<uint8_t>(abs);
            data[i+2] = static_cast<uint8_t>(abs >> 8);
            data[i+3] = static_cast<uint8_t>(abs >> 16);
            data[i+4] = static_cast<uint8_t>(abs >> 24);
            i += 4;
        }
    }
}

void inverse_bcj_filter(std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 4 < data.size(); i++) {
        if (data[i] == 0xE8 || data[i] == 0xE9) {
            int32_t abs = static_cast<int32_t>(
                static_cast<uint32_t>(data[i+1]) |
                (static_cast<uint32_t>(data[i+2]) << 8) |
                (static_cast<uint32_t>(data[i+3]) << 16) |
                (static_cast<uint32_t>(data[i+4]) << 24));
            int32_t rel = abs - static_cast<int32_t>(i) - 5;
            data[i+1] = static_cast<uint8_t>(rel);
            data[i+2] = static_cast<uint8_t>(rel >> 8);
            data[i+3] = static_cast<uint8_t>(rel >> 16);
            data[i+4] = static_cast<uint8_t>(rel >> 24);
            i += 4;
        }
    }
}

void apply_delta_filter(std::vector<uint8_t>& data) {
    if (data.empty()) return;
    uint8_t prev = 0;
    for (auto& b : data) {
        uint8_t cur = b;
        b = cur - prev;
        prev = cur;
    }
}

void inverse_delta_filter(std::vector<uint8_t>& data) {
    if (data.empty()) return;
    uint8_t acc = 0;
    for (auto& b : data) {
        acc = acc + b;
        b = acc;
    }
}
