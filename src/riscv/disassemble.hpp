#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "decode.hpp"
#include "platform.hpp"

namespace riscv {
std::string disassemble(const platform::instruction_t ins);

std::vector<std::pair<platform::instruction_t, std::string>>
disassemble_from_memory(std::span<const uint8_t> memory, uint32_t start,
                        uint32_t end);
}; // namespace riscv
