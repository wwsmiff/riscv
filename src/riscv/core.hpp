#pragma once
#include "platform.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <print>
#include <vector>

namespace riscv {
struct Core {
  uint32_t pc{platform::memory_base};
  platform::instruction_t current_ins;
  std::array<uint32_t, 32> x{};
  std::array<uint8_t, platform::memory_size> memory{};
  uint32_t memory_used{};
  bool halt{false};

  void load_bindata(const std::vector<uint8_t> &bindata);
  platform::instruction_t fetch();
  platform::instruction_t execute(); // one instruction
  void run();
};
}; // namespace riscv
