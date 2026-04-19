#include "disassemble.hpp"
#include "decode.hpp"
#include "platform.hpp"

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace riscv {
std::string disassemble(const platform::instruction_t ins) {
  std::string text{};
  constexpr auto m =
      platform::extensions & static_cast<uint8_t>(platform::Extensions::m);

  const uint32_t opcode_mask = 0x7f;
  const uint8_t opcode = ins & opcode_mask;

  switch (opcode) {
  case opcode::lui: {
    const auto decoded = decode<UType>(ins);
    const uint8_t rd = decoded.rd;
    const uint32_t imm = decoded.imm;
    text.append("lui ");
    text.append(std::format("x{}, {:x}h", rd, imm));

    break;
  }
  case opcode::auipc: {
    const UType decoded = decode<UType>(ins);
    const uint8_t rd = decoded.rd;
    const uint32_t imm = decoded.imm;

    text.append("auipc ");
    text.append(std::format("{:x}h", imm));

    break;
  }
  case opcode::jal: {
    const auto decoded = decode<JType>(ins);

    const uint32_t imm20 = decoded.imm20;
    const uint32_t imm10_1 = decoded.imm10_1;
    const uint32_t imm11 = decoded.imm11;
    const uint32_t imm19_12 = decoded.imm19_12;
    const uint8_t rd = decoded.rd >> 7;

    const uint32_t imm =
        (imm20 >> 11) | (imm19_12) | (imm11 >> 9) | (imm10_1 >> 20);

    const int32_t offset =
        (static_cast<int32_t>(imm) << 11) >> 11; // sign extend

    text.append("jal ");
    text.append(std::format("{:x}h", offset));

    break;
  }
  case opcode::jalr: {
    if constexpr (platform::verbose) {
      std::println("jalr instruction.");
    }

    const auto decoded = decode<IType>(ins);

    const uint32_t imm = decoded.imm;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t rd = decoded.rd;
    const uint8_t funct3 = decoded.funct3;

    const int32_t offset = (static_cast<int32_t>(imm) << 20) >> 20;

    text.append("jalr ");
    if (offset == 0) {
      text.append(std::format("x{}, x{}", rd, rs1));
    } else {
      text.append(std::format("x{}, {:x}h(x{})", rd, imm, rs1));
    }

    break;
  }
  case opcode::branch: {
    if constexpr (platform::verbose) {
      std::println("branch instruction.");
    }

    const auto decoded = decode<BType>(ins);

    const uint8_t rs1 = decoded.rs1;
    const uint8_t rs2 = decoded.rs2;
    const uint8_t funct3 = decoded.funct3;

    const uint32_t imm12 = decoded.imm12;
    const uint32_t imm10_5 = decoded.imm10_5;
    const uint32_t imm4_1 = decoded.imm4_1;
    const uint32_t imm11 = decoded.imm11;

    const uint32_t imm =
        (imm12 >> 19) | (imm11 << 4) | (imm10_5 >> 20) | (imm4_1 >> 8) << 1;

    int32_t offset = (static_cast<int32_t>(imm) << 19) >> 19;

    switch (funct3) {
    case 0b000: { // BEQ
      text.append("beq ");
      break;
    }
    case 0b001: { // BNE
      text.append("bne ");
      break;
    }
    case 0b100: { // BLT
      text.append("blt ");
      break;
    }
    case 0b101: { // BGE
      text.append("bge ");
      break;
    }
    case 0b110: { // BLTU
      text.append("bltu ");
      break;
    }
    case 0b111: { // BGEU
      text.append("bgeu ");
      break;
    }
    default: {
      break;
    }
    }
    text.append(std::format("x{}, x{}, {:x}h", rs1, rs2,
                            static_cast<uint32_t>(offset)));
    break;
  }
  case opcode::load: {
    if constexpr (platform::verbose) {
      std::println("load instruction.");
    }

    const auto decoded = decode<IType>(ins);

    const uint32_t imm = decoded.imm;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t funct3 = decoded.funct3;
    const uint8_t rd = decoded.rd;

    const int32_t offset = (static_cast<int32_t>(imm) << 20) >> 20;

    switch (funct3) {
    case 0b010: { // LW
      text.append("lw ");
      break;
    }
    case 0b001: { // LH
      text.append("lh ");
      break;
    }
    case 0b000: { // LB
      text.append("lb ");
      break;
    }
    case 0b100: { // LBU
      text.append("lbu ");
      break;
    }
    case 0b101: { // LHU
      text.append("lhu ");
      break;
    }
    default: {
      break;
    }
    }
    text.append(
        std::format("x{}, {:x}h(x{})", rd, static_cast<uint32_t>(offset), rs1));
    break;
  }
  case opcode::store: {
    if constexpr (platform::verbose) {
      std::println("store instruction.");
    }

    const auto decoded = decode<SType>(ins);

    const uint32_t imm11_5 = decoded.imm11_5;
    const uint8_t rs2 = decoded.rs2;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t funct3 = decoded.funct3;
    const uint32_t imm4_0 = decoded.imm4_0;

    const uint32_t imm = (imm11_5 >> 20) | (imm4_0 >> 7);
    const int32_t signed_imm = (static_cast<int32_t>(imm) << 20) >> 20;

    switch (funct3) {
    case 0b000: { // SB
      text.append("sb ");
      break;
    }
    case 0b001: { // SH
      text.append("sb ");
      break;
    }
    case 0b010: { // SW
      text.append("sb ");
      break;
    }
    default: {
      break;
    }
    }
    text.append(std::format("x{}, {:x}h(x{})", rs1, imm, rs2));
    break;
  }
  case opcode::imm_reg: {
    if constexpr (platform::verbose) {
      std::println("imm-reg instruction.");
    }

    const auto decoded = decode<IType>(ins);

    const uint32_t imm = decoded.imm;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t funct3 = decoded.funct3;
    const uint8_t rd = decoded.rd;

    const int32_t signed_imm = (static_cast<int32_t>(imm) << 20) >> 20;

    switch (funct3) {
    case 0b000: { // ADDI
      if (rs1 == 0) {
        text.append(std::format("li x{}, {:x}h", rd,
                                static_cast<uint32_t>(signed_imm)));
        return text;
      } else {
        text.append("addi ");
      }
      break;
    }
    case 0b010: { // SLTI
      text.append("slti ");
      break;
    }
    case 0b011: { // SLTIU
      text.append("sltiu ");
      break;
    }
    case 0b100: { // XORI
      text.append("xori ");
      break;
    }
    case 0b110: { // ORI
      text.append("ori ");
      break;
    }
    case 0b111: { // ANDI
      text.append("andi ");
      break;
    }
    case 0b001: { // SLLI
      text.append("slli ");
      break;
    }
    case 0b101: { // Shift right
      bool arithmetic = imm & 0b010000000000;
      if (arithmetic) { // SRAI
        text.append("srai ");
      } else { // SRLI
        text.append("srli ");
      }
      break;
    }
    }

    text.append(std::format("x{}, x{}, {:x}h", rd, rs1, imm));
    break;
  }
  case opcode::reg_reg: {
    const auto decoded = decode<RType>(ins);

    const uint8_t funct7 = decoded.funct7;
    const uint8_t rs2 = decoded.rs2;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t funct3 = decoded.funct3;
    const uint8_t rd = decoded.rd;

    switch (funct3) {
    case 0b000: {
      if (funct7 == 0) { // ADD
        text.append("add ");
      } else if (funct7 == 0x20) { // SUB
        text.append("sub ");
      }
      if constexpr (m) {
        if (funct7 == 1) { // MUL
          text.append("mul ");
        }
      }
      break;
    }
    case 0b001: { // SLL
      if (funct7 == 0) {
        text.append("sll ");
      }
      if constexpr (m) {
        if (funct7 == 1) { // MULH
          text.append("mulh ");
        }
      }
      break;
    }
    case 0b010: { // SLT
      if (funct7 == 0) {
        text.append("slt ");
      }
      if constexpr (m) { // MULHSU
        if (funct7 == 1) {
          text.append("mulhsu ");
        }
      }
      break;
    }
    case 0b011: { // SLTU
      if (funct7 == 0) {
        text.append("sltu ");
      }
      if constexpr (m) {
        if (funct7 == 1) { // MULHU
          text.append("mulhu ");
        }
      }
      break;
    }
    case 0b100: { // XOR
      if (funct7 == 0) {
        text.append("xor ");
      }
      if constexpr (m) {
        if (funct7 == 1) { // DIV
          text.append("div ");
        }
      }
      break;
    }
    case 0b101: {
      if (funct7 == 0) { // SRL
        text.append("srl ");
      } else if (funct7 == 0x20) { // SRA
        text.append("sra ");
      }
      if constexpr (m) {
        if (funct7 == 1) {
          text.append("divu ");
        }
      }
      break;
    }
    case 0b110: { // OR
      if (funct7 == 0) {
        text.append("or ");
      }
      if constexpr (m) { // REM
        if (funct7 == 1) {
          text.append("or ");
        }
      }
      break;
    }
    case 0b111: { // AND
      if (funct7 == 0) {
        text.append("and ");
      }
      if constexpr (m) { // REMU
        if (funct7 == 1) {
          text.append("remu ");
        }
      }
      break;
    }
    }

    text.append(std::format("x{}, x{}, x{}", rd, rs1, rs2));
    break;
  }
  case opcode::fence: {
    text.append("fence instruction unimplemented");
    break;
  }
  case opcode::system: {
    const auto decoded = decode<IType>(ins);

    [[maybe_unused]] const uint32_t imm = decoded.imm;
    [[maybe_unused]] const uint8_t rs1 = decoded.rs1;
    [[maybe_unused]] const uint8_t funct3 = decoded.funct3;
    [[maybe_unused]] const uint8_t rd = decoded.rd;

    if (imm == 0) { // ecall
      text.append("ecall");
    } else {
      text.append("system instruction unimplemented");
    }
    break;
  }

  default: {
    text.append("unimplemented instruction");
    break;
  }
  }

  return text;
}

std::vector<std::pair<platform::instruction_t, std::string>>
disassemble_from_memory(std::span<const uint8_t> memory, uint32_t start,
                        uint32_t end) {
  std::vector<std::pair<platform::instruction_t, std::string>> disassembly{};
  for (uint32_t i{start}; i < end; i += 4) {
    const uint8_t byte0 = memory[i + 0];
    const uint8_t byte1 = memory[i + 1];
    const uint8_t byte2 = memory[i + 2];
    const uint8_t byte3 = memory[i + 3];

    const uint16_t parcel0 = static_cast<uint16_t>((byte1 << 8) | byte0);
    const uint16_t parcel1 = static_cast<uint16_t>((byte3 << 8) | byte2);

    platform::instruction_t ins =
        static_cast<platform::instruction_t>((parcel1 << 16) | parcel0);

    if (ins == 0x00000000 || ins == 0xffffffff) {
      break;
    }

    disassembly.emplace_back(ins, disassemble(ins));
  }

  return disassembly;
}
}; // namespace riscv
