#include <cstdint>
#include <print>

namespace riscv {
using opcode_t = uint8_t;
namespace platform {
constexpr auto ialign{32};
constexpr auto ilen{32};
constexpr auto allow_misaligned_accesses{false};
} // namespace platform

/* clang-format off */
namespace opcode {
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
constexpr opcode_t store	 = 0b0'0000011;

// arithmetic instructions imm-reg
// addi, slti, sltiu, xori, ori, andi, slli, srli, srai
constexpr opcode_t imm_reg = 0b0'0010011;

// arithmetic instructions reg-reg
// add, sub, sll, slt, sltu, xor, srl, sra, or, and
constexpr opcode_t reg_reg = 0b0'0110011;

// fence
constexpr opcode_t fence	 = 0b0'0001111;

// system
constexpr opcode_t system  = 0b0'1110011;

} // namespace op
/* clang-format on */
}; // namespace riscv

struct Hart {
  std::array<uint32_t, 32> x{};
  uint32_t pc{};
};

int main() {
  std::println("Hello, RISC-V!");
  return 0;
}
