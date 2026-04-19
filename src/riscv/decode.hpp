#pragma once

#include <concepts>
#include <cstdint>
#include <print>
#include <type_traits>

#include "platform.hpp"

namespace riscv {
struct InstructionType {};

struct RType final : InstructionType {
  uint8_t funct7{};
  uint8_t rs2{};
  uint8_t rs1{};
  uint8_t funct3{};
  uint8_t rd{};
  uint8_t opcode{};
};

struct IType final : InstructionType {
  uint32_t imm{};
  uint8_t rs1{};
  uint8_t funct3{};
  uint8_t rd{};
  int32_t signed_imm{};
  uint8_t opcode{};
};

struct SType final : InstructionType {
  uint32_t imm11_5{};
  uint8_t rs2{};
  uint8_t rs1{};
  uint8_t funct3{};
  uint32_t imm4_0{};
  uint8_t opcode{};
};

struct BType final : InstructionType {
  uint32_t imm12{};
  uint32_t imm10_5{};
  uint8_t rs1{};
  uint8_t rs2{};
  uint8_t funct3{};
  uint32_t imm4_1{};
  uint32_t imm11{};
  uint8_t opcode{};
};

struct UType final : InstructionType {
  uint32_t imm{};
  uint8_t rd{};
  uint8_t opcode{};
};

struct JType final : InstructionType {
  uint32_t imm20{};
  uint32_t imm10_1{};
  uint32_t imm11{};
  uint32_t imm19_12{};
  uint8_t rd{};
  uint8_t opcode{};
};

template <typename T>
concept InstructionT = std::is_base_of<InstructionType, T>::value;

template <InstructionT I>
constexpr inline const I decode(platform::instruction_t ins) {
  I decoded{};
  const uint32_t opcode_mask = 0x7f;
  decoded.opcode = ins & opcode_mask;

  if constexpr (std::is_same<I, RType>::value) {
    const uint32_t funct7_mask = 0xfe000000;
    const uint32_t rs2_mask = 0x01f00000;
    const uint32_t rs1_mask = 0x000f8000;
    const uint32_t funct3_mask = 0x00007000;
    const uint32_t rd_mask = 0x00000f80;

    decoded.funct7 = (ins & funct7_mask) >> 25;
    decoded.rs2 = (ins & rs2_mask) >> 20;
    decoded.rs1 = (ins & rs1_mask) >> 15;
    decoded.funct3 = (ins & funct3_mask) >> 12;
    decoded.rd = (ins & rd_mask) >> 7;
  } else if constexpr (std::is_same<I, IType>::value) {
    const uint32_t imm_mask = 0xfff00000;
    const uint32_t rs1_mask = 0x000f8000;
    const uint32_t funct3_mask = 0x00007000;
    const uint32_t rd_mask = 0x00000f80;

    decoded.imm = (ins & imm_mask) >> 20;
    decoded.rs1 = (ins & rs1_mask) >> 15;
    decoded.funct3 = (ins & funct3_mask) >> 12;
    decoded.rd = (ins & rd_mask) >> 7;
  } else if constexpr (std::is_same<I, SType>::value) {
    const uint32_t imm11_5_mask = 0xfe000000;
    const uint32_t rs2_mask = 0x01f00000;
    const uint32_t rs1_mask = 0x000f8000;
    const uint32_t funct3_mask = 0x00007000;
    const uint32_t imm4_0_mask = 0x00000f80;

    decoded.imm11_5 = (ins & imm11_5_mask);
    decoded.rs2 = (ins & rs2_mask) >> 20;
    decoded.rs1 = (ins & rs1_mask) >> 15;
    decoded.funct3 = (ins & funct3_mask) >> 12;
    decoded.imm4_0 = (ins & imm4_0_mask);
  } else if constexpr (std::is_same<I, BType>::value) {
    const uint32_t imm12_mask = 0x80000000;
    const uint32_t imm10_5_mask = 0x7e000000;
    const uint32_t rs2_mask = 0x01f00000;
    const uint32_t rs1_mask = 0x000f8000;
    const uint32_t funct3_mask = 0x00007000;
    const uint32_t imm4_1_mask = 0x00000f00;
    const uint32_t imm11_mask = 0x00000080;

    decoded.rs1 = (ins & rs1_mask) >> 15;
    decoded.rs2 = (ins & rs2_mask) >> 20;
    decoded.funct3 = (ins & funct3_mask) >> 12;

    decoded.imm12 = ins & imm12_mask;
    decoded.imm10_5 = ins & imm10_5_mask;
    decoded.imm4_1 = ins & imm4_1_mask;
    decoded.imm11 = ins & imm11_mask;
  } else if constexpr (std::is_same<I, UType>::value) {
    const uint32_t rd_mask = 0xf80;
    const uint32_t imm_mask = 0xfffff000;
    decoded.rd = (ins & rd_mask) >> 7;
    decoded.imm = ins & imm_mask;
  } else if constexpr (std::is_same<I, JType>::value) {
    const uint32_t imm20_mask = 0x80000000;    // [31]
    const uint32_t imm10_1_mask = 0x7fe00000;  // [30:21]
    const uint32_t imm11_mask = 0x00100000;    // [20]
    const uint32_t imm19_12_mask = 0x000ff000; // [19:12]
    const uint32_t rd_mask = 0xf80;

    decoded.imm20 = ins & imm20_mask;
    decoded.imm10_1 = ins & imm10_1_mask;
    decoded.imm11 = ins & imm11_mask;
    decoded.imm19_12 = ins & imm19_12_mask;
    decoded.rd = (ins & rd_mask) >> 7;
  }

  return decoded;
}

}; // namespace riscv
