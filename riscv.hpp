#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace riscv {
namespace platform {
using opcode_t = uint8_t;
using instruction_t = uint32_t;

constexpr uint32_t ialign{32};
constexpr uint32_t ilen{32};
constexpr uint32_t allow_misaligned_accesses{false};
constexpr uint32_t memory_size{2 << 20}; // 2 MiB.
constexpr uint32_t memory_base{0x80000000};
constexpr bool verbose{false};
}; // namespace platform

/* clang-format off */
namespace opcode {
using opcode_t = platform::opcode_t;
constexpr opcode_t lui		 = 0b0'0110111;
constexpr opcode_t auipc 	 = 0b0'0010111;
constexpr opcode_t jal	 	 = 0b0'1101111;
constexpr opcode_t jalr	 	 = 0b0'1100111;

// branching instructions
// beq, bne, blt, bge, bltu, bgeu
constexpr opcode_t branch	 = 0b0'1100011;

// load instructions
// lb, lh, lw, lbu, lhu, 
constexpr opcode_t load		 = 0b0'0000011;

// store instructions
// sb, sh, sw
constexpr opcode_t store	 = 0b0'0100011;

// arithmetic and logic instructions imm-reg
// addi, slti, sltiu, xori, ori, andi, slli, srli, srai
constexpr opcode_t imm_reg = 0b0'0010011;

// arithmetic and logic instructions reg-reg
// add, sub, sll, slt, sltu, xor, srl, sra, or, and
constexpr opcode_t reg_reg = 0b0'0110011;

// fence
constexpr opcode_t fence	 = 0b0'0001111;

// system
constexpr opcode_t system  = 0b0'1110011;
};
/* clang-format on */

struct Core {
  uint32_t pc{platform::memory_base};
  std::array<uint32_t, 32> x{};
  std::array<uint8_t, platform::memory_size> memory{};
  void load_binfile(const std::vector<uint8_t> &bindata);
  platform::instruction_t fetch();
  void execute();
};
}; // namespace riscv
