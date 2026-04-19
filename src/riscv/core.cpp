#include "core.hpp"
#include "decode.hpp"
#include "disassemble.hpp"
#include "platform.hpp"
#include <cstdint>
#include <print>
#include <vector>

namespace riscv {

std::vector<uint8_t> bindata(const std::filesystem::path &fpath) {
  std::ifstream binfile(fpath, std::ios::binary);
  const auto fsize = std::filesystem::file_size(fpath);
  if (fsize == 0) {
    std::println(stderr, "Specified file has size 0.");
    return {};
  }

  std::vector<uint8_t> fdata(fsize);
  binfile.read(reinterpret_cast<char *>(fdata.data()), fsize);
  binfile.close();
  return fdata;
}

void Core::load_bindata(const std::vector<uint8_t> &bindata) {
  std::copy(bindata.cbegin(), bindata.cend(), memory.begin());
  std::println("{}/{} bytes used.", bindata.size(), platform::memory_size);
  memory_used = bindata.size();
  memory.at(bindata.size()) = 0xff;
  memory.at(bindata.size() + 1) = 0xff;
  memory.at(bindata.size() + 2) = 0xff;
  memory.at(bindata.size() + 3) = 0xff;
}

platform::instruction_t Core::fetch() {
  platform::instruction_t ins{};
  const uint8_t byte0 = memory.at(pc - platform::memory_base);
  const uint8_t byte1 = memory.at((pc - platform::memory_base) + 1);
  const uint8_t byte2 = memory.at((pc - platform::memory_base) + 2);
  const uint8_t byte3 = memory.at((pc - platform::memory_base) + 3);

  const uint16_t parcel0 = static_cast<uint16_t>((byte1 << 8) | byte0);
  const uint16_t parcel1 = static_cast<uint16_t>((byte3 << 8) | byte2);

  ins = static_cast<platform::instruction_t>((parcel1 << 16) | parcel0);
  pc += 4;

  return ins;
}

platform::instruction_t Core::execute() {
  constexpr auto m =
      platform::extensions & static_cast<uint8_t>(platform::Extensions::m);
  platform::instruction_t ins = fetch();
  if (ins == 0x00000000 || ins == 0xffffffff) {
    halt = true;
  }
  current_ins = ins;

  if constexpr (platform::verbose) {
    const auto text = disassemble(ins);
    std::println("<{:08x}> {:08x} {}", pc, ins, text);
  }

  x.at(0) = 0;
  const uint32_t opcode_mask = 0x7f;
  const uint8_t opcode = ins & opcode_mask;

  switch (opcode) {
  case opcode::lui: {
    const auto decoded = decode<UType>(ins);
    const uint8_t rd = decoded.rd;
    const uint32_t imm = decoded.imm;
    x.at(rd) = imm;
    break;
  }
  case opcode::auipc: {
    const auto decoded = decode<UType>(ins);
    const uint8_t rd = decoded.rd;
    const uint32_t imm = decoded.imm;

    x.at(rd) = (pc - 4) + imm;
    break;
  }
  case opcode::jal: {
    const auto decoded = decode<JType>(ins);

    const uint32_t imm20 = decoded.imm20;
    const uint32_t imm10_1 = decoded.imm10_1;
    const uint32_t imm11 = decoded.imm11;
    const uint32_t imm19_12 = decoded.imm19_12;
    const uint8_t rd = decoded.rd;

    const uint32_t imm =
        (imm20 >> 11) | (imm19_12) | (imm11 >> 9) | (imm10_1 >> 20);

    const int32_t offset =
        (static_cast<int32_t>(imm) << 11) >> 11; // sign extend
    const uint32_t target_address = (pc - 4) + offset;
    if ((target_address & 0x3) != 0) {
      std::println(stderr,
                   "Target address for JAL<0x{:08x}> is not 4-byte aligned.",
                   (pc - 4));
      return {};
    }

    x.at(rd) = pc;
    pc = target_address;
    break;
  }
  case opcode::jalr: {
    const auto decoded = decode<IType>(ins);

    const uint32_t imm = decoded.imm;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t rd = decoded.rd;
    const uint8_t funct3 = decoded.funct3;

    if (funct3 != 0) {
      std::println(stderr, "Illegal jump instruction at <0x{:08x}>", (pc - 4));
      return {};
    }
    const int32_t offset = (static_cast<int32_t>(imm) << 20) >> 20;
    const uint32_t target_address =
        (static_cast<int32_t>(x.at(rs1)) + offset) & ~1;

    if ((target_address & 0x3) != 0) {
      std::println(stderr,
                   "Target address for JALR<0x{:08x}> is not 4-byte aligned.",
                   (pc - 4));

      return {};
    }
    x.at(rd) = pc;
    pc = target_address;
    break;
  }
  case opcode::branch: {
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
      if (static_cast<int32_t>(x.at(rs1)) == static_cast<int32_t>(x.at(rs2))) {
        pc = (pc - 4) + offset;
      }
      break;
    }
    case 0b001: { // BNE
      if (static_cast<int32_t>(x.at(rs1)) != static_cast<int32_t>(x.at(rs2))) {
        pc = (pc - 4) + offset;
      }
      break;
    }
    case 0b100: { // BLT
      if (static_cast<int32_t>(x.at(rs1)) < static_cast<int32_t>(x.at(rs2))) {
        pc = (pc - 4) + offset;
      }
      break;
    }
    case 0b101: { // BGE
      if (static_cast<int32_t>(x.at(rs1)) >= static_cast<int32_t>(x.at(rs2))) {
        pc = (pc - 4) + offset;
      }
      break;
    }
    case 0b110: { // BLTU
      if (x.at(rs1) < x.at(rs2)) {
        pc = (pc - 4) + offset;
      }
      break;
    }
    case 0b111: { // BGEU
      if (x.at(rs1) >= x.at(rs2)) {
        pc = (pc - 4) + offset;
      }
      break;
    }
    default: {
      std::println(stderr, "Illegal branch instruction at <0x{:08x}>",
                   (pc - 4));
      break;
    }
    }
    break;
  }
  case opcode::load: {
    const auto decoded = decode<IType>(ins);

    const uint32_t imm = decoded.imm;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t funct3 = decoded.funct3;
    const uint8_t rd = decoded.rd;

    const int32_t offset = (static_cast<int32_t>(imm) << 20) >> 20;

    const uint32_t target_address =
        (x.at(rs1) + offset) - platform::memory_base;

    switch (funct3) {
    case 0b010: { // LW
      uint32_t byte0 = memory.at(target_address);
      uint32_t byte1 = memory.at(target_address + 1) << 8;
      uint32_t byte2 = memory.at(target_address + 2) << 16;
      uint32_t byte3 = memory.at(target_address + 3) << 24;
      x.at(rd) = byte3 | byte2 | byte1 | byte0;
      break;
    }
    case 0b001: { // LH
      const uint16_t byte0 = memory.at(target_address);
      const uint16_t byte1 = memory.at(target_address + 1) << 8;
      const int16_t halfword = byte1 | byte0;
      const int32_t word = (halfword << 16) >> 16;
      x.at(rd) = word;
      break;
    }
    case 0b000: { // LB
      const int8_t byte = memory.at(target_address);
      const int32_t word = (byte << 24) >> 24;
      x.at(rd) = word;
      break;
    }
    case 0b100: { // LBU
      const uint8_t byte = memory.at(target_address);
      x.at(rd) = byte;
      break;
    }
    case 0b101: { // LHU
      const uint16_t byte0 = memory.at(target_address);
      const uint16_t byte1 = memory.at(target_address + 1) << 8;
      const uint16_t halfword = byte1 | byte0;
      x.at(rd) = static_cast<uint32_t>(halfword);
      break;
    }
    default: {
      std::println(stderr, "Illegal load instructiion at <0x{:08x}>", (pc - 4));
      break;
    }
    }
    break;
  }
  case opcode::store: {
    const auto decoded = decode<SType>(ins);

    const uint32_t imm11_5 = decoded.imm11_5;
    const uint8_t rs2 = decoded.rs2;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t funct3 = decoded.funct3;
    const uint32_t imm4_0 = decoded.imm4_0;

    const uint32_t imm = (imm11_5 >> 20) | (imm4_0 >> 7);
    const int32_t signed_imm = (static_cast<int32_t>(imm) << 20) >> 20;

    const uint32_t target_address =
        (x.at(rs1) + signed_imm) - platform::memory_base;

    switch (funct3) {
    case 0b000: { // SB
      const uint8_t b = x.at(rs2) & 0xff;
      memory.at(target_address) = b;
      break;
    }
    case 0b001: { // SH
      const uint16_t hw = x.at(rs2) & 0xffff;
      const uint8_t b1 = (hw & 0xff00) >> 8;
      const uint8_t b0 = (hw & 0x00ff);
      memory.at(target_address) = b0;
      memory.at(target_address + 1) = b1;
      break;
    }
    case 0b010: { // SW
      const uint32_t w = x.at(rs2);
      const uint32_t b3 = (w & 0xff000000) >> 24;
      const uint32_t b2 = (w & 0x00ff0000) >> 16;
      const uint32_t b1 = (w & 0x0000ff00) >> 8;
      const uint32_t b0 = (w & 0x000000ff);

      memory.at(target_address) = b0;
      memory.at(target_address + 1) = b1;
      memory.at(target_address + 2) = b2;
      memory.at(target_address + 3) = b3;
      break;
    }
    default: {
      std::println(stderr, "Illegal store instruction at <0x{:08x}>", (pc - 4));
      break;
    }
    }
    break;
  }
  case opcode::imm_reg: {
    const auto decoded = decode<IType>(ins);

    const uint32_t imm = decoded.imm;
    const uint8_t rs1 = decoded.rs1;
    const uint8_t funct3 = decoded.funct3;
    const uint8_t rd = decoded.rd;

    const int32_t signed_imm = (static_cast<int32_t>(imm) << 20) >> 20;

    switch (funct3) {
    case 0b000: { // ADDI
      x.at(rd) = x.at(rs1) + signed_imm;
      break;
    }
    case 0b010: { // SLTI
      x.at(rd) = static_cast<int32_t>(x.at(rs1)) < signed_imm;
      break;
    }
    case 0b011: { // SLTIU
      x.at(rd) = x.at(rs1) < static_cast<uint32_t>(signed_imm);
      break;
    }
    case 0b100: { // XORI
      x.at(rd) = x.at(rs1) ^ signed_imm;
      break;
    }
    case 0b110: { // ORI
      x.at(rd) = x.at(rs1) | signed_imm;
      break;
    }
    case 0b111: { // ANDI
      x.at(rd) = x.at(rs1) & signed_imm;
      break;
    }
    case 0b001: { // SLLI
      uint8_t shiftamt = imm & 0x1f;
      x.at(rd) = x.at(rs1) << shiftamt;
      break;
    }
    case 0b101: { // Shift right
      uint8_t shiftamt = imm & 0x1f;
      bool arithmetic = imm & 0b010000000000;
      if (arithmetic) { // SRAI
        x.at(rd) = static_cast<int32_t>(x.at(rs1)) >> shiftamt;
      } else { // SRLI
        x.at(rd) = x.at(rs1) >> shiftamt;
      }
      break;
    }
    }
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
        x.at(rd) = x.at(rs1) + x.at(rs2);
      } else if (funct7 == 0x20) { // SUB
        x.at(rd) = x.at(rs1) - x.at(rs2);
      }
      if constexpr (m) {
        if (funct7 == 1) { // MUL
          const uint64_t product = x.at(rs1) * x.at(rs2);
          x.at(rd) = static_cast<uint32_t>(product & 0xffffffff);
        }
      }
      break;
    }
    case 0b001: { // SLL
      if (funct7 == 0) {
        const uint8_t shiftamt = x.at(rs2) & 0x1f;
        x.at(rd) = x.at(rs1) << shiftamt;
      }
      if constexpr (m) {
        if (funct7 == 1) { // MULH
          const int64_t signed_rs1 =
              (static_cast<int64_t>(x.at(rs1)) << 32) >> 32;
          const int64_t signed_rs2 =
              (static_cast<int64_t>(x.at(rs2)) << 32) >> 32;
          const int64_t product = signed_rs1 * signed_rs2;
          x.at(rd) = product >> 32;
        }
      }
      break;
    }
    case 0b010: { // SLT
      if (funct7 == 0) {
        x.at(rd) =
            static_cast<int32_t>(x.at(rs1)) < static_cast<int32_t>(x.at(rs2));
      }
      if constexpr (m) { // MULHSU
        if (funct7 == 1) {
          const int64_t signed_rs1 =
              (static_cast<int64_t>(x.at(rs1)) << 32) >> 32;
          const int64_t product = signed_rs1 * static_cast<uint64_t>(x.at(rs2));
          x.at(rd) = product >> 32;
        }
      }
      break;
    }
    case 0b011: { // SLTU
      if (funct7 == 0) {
        x.at(rd) = x.at(rs1) < x.at(rs2);
      }
      if constexpr (m) {
        if (funct7 == 1) { // MULHU
          const uint64_t product = static_cast<uint64_t>(x.at(rs1)) *
                                   static_cast<uint64_t>(x.at(rs2));
          x.at(rd) = product >> 32;
        }
      }
      break;
    }
    case 0b100: { // XOR
      if (funct7 == 0) {
        x.at(rd) = x.at(rs1) ^ x.at(rs2);
      }
      if constexpr (m) {
        if (funct7 == 1) { // DIV
          if (static_cast<int32_t>(x.at(rs1)) ==
                  std::numeric_limits<int32_t>::min() &&
              static_cast<int32_t>(x.at(rs2)) == -1) {
            x.at(rd) = std::numeric_limits<int32_t>::min();
          } else if (x.at(rs2) == 0) {
            x.at(rd) = 0xffffffff;
          } else {
            const int32_t quotient = static_cast<int32_t>(x.at(rs1)) /
                                     static_cast<int32_t>(x.at(rs2));
            x.at(rd) = quotient;
          }
        }
      }
      break;
    }
    case 0b101: {
      const uint8_t shiftamt = x.at(rs2) & 0x1f;
      if (funct7 == 0) { // SRL
        x.at(rd) = x.at(rs1) >> shiftamt;
      } else if (funct7 == 0x20) { // SRA
        x.at(rd) = static_cast<int32_t>(x.at(rs1)) >> shiftamt;
      }
      if constexpr (m) {
        if (funct7 == 1) {
          if (x.at(rs2) == 0) { // DIVU
            x.at(rd) = 0xffffffff;
          } else {
            uint32_t quotient = static_cast<uint32_t>(x.at(rs1)) /
                                static_cast<uint32_t>(x.at(rs2));
            x.at(rd) = quotient;
          }
        }
      }
      break;
    }
    case 0b110: { // OR
      if (funct7 == 0) {
        x.at(rd) = x.at(rs1) | x.at(rs2);
      }
      if constexpr (m) { // REM
        if (funct7 == 1) {
          if (static_cast<int32_t>(x.at(rs1)) ==
                  std::numeric_limits<int32_t>::min() &&
              static_cast<int32_t>(x.at(rs2)) == -1) {
            x.at(rd) = 0;
          } else if (x.at(rs2) == 0) {
            x.at(rd) = x.at(rs1);
            break;
          } else {
            int32_t rem = static_cast<int32_t>(x.at(rs1)) %
                          static_cast<int32_t>(x.at(rs2));
            x.at(rd) = rem;
          }
        }
      }
      break;
    }
    case 0b111: { // AND
      if (funct7 == 0) {
        x.at(rd) = x.at(rs1) & x.at(rs2);
      }
      if constexpr (m) { // REMU
        if (funct7 == 1) {
          if (x.at(rs2) == 0) {
            x.at(rd) = x.at(rs1);
            break;
          }
          uint32_t rem = static_cast<uint32_t>(x.at(rs1)) %
                         static_cast<uint32_t>(x.at(rs2));
          x.at(rd) = rem;
        }
      }
      break;
    }
    }
    break;
  }
  case opcode::fence: {
    break;
  }
  case opcode::system: {
    const auto decoded = decode<IType>(ins);

    [[maybe_unused]] const uint32_t imm = decoded.imm;
    [[maybe_unused]] const uint8_t rs1 = decoded.rs1;
    [[maybe_unused]] const uint8_t funct3 = decoded.funct3;
    [[maybe_unused]] const uint8_t rd = decoded.rd;

    if (imm == 0) { // ecall
      current_ins = {};
      return {};
    }
    break;
  }

  default: {
    break;
  }
  }

  return ins;
}

void Core::run() {
  while (!halt) {
    execute();
  }
}

}; // namespace riscv
