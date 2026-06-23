#pragma once
#include <cstdint>
#include <vector>

void apply_bcj_filter(std::vector<uint8_t>& data);
void inverse_bcj_filter(std::vector<uint8_t>& data);
void apply_delta_filter(std::vector<uint8_t>& data);
void inverse_delta_filter(std::vector<uint8_t>& data);
