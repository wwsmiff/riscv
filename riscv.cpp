#include "riscv.hpp"
#include <cstdint>
#include <print>
#include <vector>

namespace riscv {
void Core::load_binfile(const std::vector<uint8_t> &bindata) {
  std::copy(bindata.cbegin(), bindata.cend(), memory.begin());
  std::println("{}/{} bytes used.", bindata.size(), platform::memory_size);
  memory.at(bindata.size()) = 0xff;
  memory.at(bindata.size() + 1) = 0xff;
  memory.at(bindata.size() + 2) = 0xff;
  memory.at(bindata.size() + 3) = 0xff;

  std::println("Instructions in memory:");
  for (size_t i{}; i < memory.size(); i += 4) {
    platform::instruction_t ins{};

    const uint8_t byte0 = memory.at(i);
    const uint8_t byte1 = memory.at(i + 1);
    const uint8_t byte2 = memory.at(i + 2);
    const uint8_t byte3 = memory.at(i + 3);

    const uint16_t parcel0 = static_cast<uint16_t>((byte1 << 8) | byte0);
    const uint16_t parcel1 = static_cast<uint16_t>((byte3 << 8) | byte2);

    ins = static_cast<platform::instruction_t>((parcel1 << 16) | parcel0);

    if (ins != 0x00000000 && ins != 0xffffffff) {
      std::println("<0x{:08x}>:\t0x{:08x}", i + platform::memory_base, ins);
    }
  }
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

void Core::execute() {
  platform::instruction_t ins = fetch();
  while (ins != 0xffffffff && ins != 0x00000000) {
    x.at(0) = 0;
    std::print("<0x{:08x}> 0x{:08x}: ", (pc - 4), ins);
    const uint32_t opcode_mask = 0x7f;
    const uint8_t opcode = ins & opcode_mask;

    if (opcode == opcode::lui) {
      std::println("lui instruction.");
      const uint32_t rd_mask = 0xf80;
      const uint32_t imm_mask = 0xfffff000;
      const uint8_t rd = (ins & rd_mask) >> 7;
      const uint32_t imm = ins & imm_mask;
      x.at(rd) = imm;
    } else if (opcode == opcode::auipc) {
      std::println("auipc instruction.");
      const uint32_t rd_mask = 0xf80;
      const uint32_t imm_mask = 0xfffff000;

      const uint8_t rd = (ins & rd_mask) >> 7;
      const uint32_t imm = ins & imm_mask;

      x.at(rd) = (pc - 4) + imm;

    } else if (opcode == opcode::jal) {
      std::println("jal instruction.");
      const uint32_t imm20_mask = 0x80000000;    // [31]
      const uint32_t imm10_1_mask = 0x7fe00000;  // [30:21]
      const uint32_t imm11_mask = 0x00100000;    // [20]
      const uint32_t imm19_12_mask = 0x000ff000; // [19:12]
      const uint32_t rd_mask = 0xf80;

      const uint32_t imm20 = ins & imm20_mask;
      const uint32_t imm10_1 = ins & imm10_1_mask;
      const uint32_t imm11 = ins & imm11_mask;
      const uint32_t imm19_12 = ins & imm19_12_mask;
      const uint8_t rd = (ins & rd_mask) >> 7;

      const uint32_t imm =
          (imm20 >> 11) | (imm19_12) | (imm11 >> 9) | (imm10_1 >> 20);

      x.at(rd) = pc;

      const int32_t offset =
          (static_cast<int32_t>(imm) << 11) >> 11; // sign extend
      const uint32_t target_address = (pc - 4) + offset;
      if ((target_address & 0x3) != 0) {
        std::println(stderr,
                     "Target address for JAL<0x{:08x}> is not 4-byte aligned.",
                     (pc - 4));
        return;
      }

      pc = target_address;
    } else if (opcode == opcode::jalr) {
      std::println("jalr instruction.");
      const uint32_t imm_mask = 0xfff00000;
      const uint32_t rs1_mask = 0x000f8000;
      const uint32_t funct3_mask = 0x00007000;
      const uint32_t rd_mask = 0xf80;

      const uint32_t imm = (ins & imm_mask) >> 20;
      const uint8_t rs1 = (ins & rs1_mask) >> 15;
      const uint8_t rd = (ins & rd_mask) >> 7;
      const uint8_t funct3 = (ins & funct3_mask) >> 12;
      if (funct3 != 0) {
        std::println(stderr, "Illegal jump instruction at <0x{:08x}>",
                     (pc - 4));
        return;
      }
      const int32_t offset = (static_cast<int32_t>(imm) << 20) >> 20;
      const uint32_t target_address =
          (static_cast<int32_t>(x.at(rs1)) + offset) & ~1;

      if ((target_address & 0x3) != 0) {
        std::println(stderr,
                     "Target address for JALR<0x{:08x}> is not 4-byte aligned.",
                     (pc - 4));

        return;
      }
      x.at(rd) = pc;
      pc = target_address;
    } else if (opcode == opcode::branch) {
      std::println("branch instruction.");
      const uint32_t imm12_mask = 0x80000000;
      const uint32_t imm10_5_mask = 0x7e000000;
      const uint32_t rs2_mask = 0x01f00000;
      const uint32_t rs1_mask = 0x000f8000;
      const uint32_t funct3_mask = 0x00007000;
      const uint32_t imm4_1_mask = 0x00000f00;
      const uint32_t imm11_mask = 0x00000080;

      const uint8_t rs1 = (ins & rs1_mask) >> 15;
      const uint8_t rs2 = (ins & rs2_mask) >> 20;
      const uint8_t funct3 = (ins & funct3_mask) >> 12;

      const uint32_t imm12 = ins & imm12_mask;
      const uint32_t imm10_5 = ins & imm10_5_mask;
      const uint32_t imm4_1 = ins & imm4_1_mask;
      const uint32_t imm11 = ins & imm11_mask;

      const uint32_t imm =
          (imm12 >> 19) | (imm11 << 4) | (imm10_5 >> 20) | (imm4_1 >> 8) << 1;

      int32_t offset = (static_cast<int32_t>(imm) << 19) >> 19;

      if (funct3 == 0b000) { // BEQ
        if (static_cast<int32_t>(x.at(rs1)) ==
            static_cast<int32_t>(x.at(rs2))) {
          pc = (pc - 4) + offset;
        }
      } else if (funct3 == 0b001) { // BNE
        if (static_cast<int32_t>(x.at(rs1)) !=
            static_cast<int32_t>(x.at(rs2))) {
          pc = (pc - 4) + offset;
        }
      } else if (funct3 == 0b100) { // BLT
        if (static_cast<int32_t>(x.at(rs1)) < static_cast<int32_t>(x.at(rs2))) {
          pc = (pc - 4) + offset;
        }
      } else if (funct3 == 0b101) { // BGE
        if (static_cast<int32_t>(x.at(rs1)) >=
            static_cast<int32_t>(x.at(rs2))) {
          pc = (pc - 4) + offset;
        }
      } else if (funct3 == 0b110) { // BLTU
        if (x.at(rs1) < x.at(rs2)) {
          pc = (pc - 4) + offset;
        }
      } else if (funct3 == 0b111) { // BGEU
        if (x.at(rs1) >= x.at(rs2)) {
          pc = (pc - 4) + offset;
        }
      } else {
        std::println(stderr, "Illegal branch instruction at <0x{:08x}>",
                     (pc - 4));
      }
    } else if (opcode == opcode::load) {
      std::println("load instruction.");
      const uint32_t imm_mask = 0xfff00000;
      const uint32_t rs1_mask = 0x000f8000;
      const uint32_t funct3_mask = 0x00007000;
      const uint32_t rd_mask = 0x00000f80;

      const uint32_t imm = (ins & imm_mask) >> 20;
      const uint8_t rs1 = (ins & rs1_mask) >> 15;
      const uint8_t funct3 = (ins & funct3_mask) >> 12;
      const uint8_t rd = (ins & rd_mask) >> 7;
      const int32_t offset = (static_cast<int32_t>(imm) << 20) >> 20;

      const uint32_t target_address =
          (x.at(rs1) + offset) - platform::memory_base;

      if (funct3 == 0b010) { // LW
        uint32_t byte0 = memory.at(target_address);
        uint32_t byte1 = memory.at(target_address + 1) << 8;
        uint32_t byte2 = memory.at(target_address + 2) << 16;
        uint32_t byte3 = memory.at(target_address + 3) << 24;
        x.at(rd) = byte3 | byte2 | byte1 | byte0;
      } else if (funct3 == 0b001) { // LH
        const uint16_t byte0 = memory.at(target_address);
        const uint16_t byte1 = memory.at(target_address + 1) << 8;
        const int16_t halfword = byte1 | byte0;
        const int32_t word = (halfword << 16) >> 16;
        x.at(rd) = word;
      } else if (funct3 == 0b000) { // LB
        const int8_t byte = memory.at(target_address);
        const int32_t word = (byte << 24) >> 24;
        x.at(rd) = word;
      } else if (funct3 == 0b100) { // LBU
        const uint8_t byte = memory.at(target_address);
        x.at(rd) = byte;
      } else if (funct3 == 0b101) { // LHU
        const uint16_t byte0 = memory.at(target_address);
        const uint16_t byte1 = memory.at(target_address + 1) << 8;
        const uint16_t halfword = byte1 | byte0;
        x.at(rd) = static_cast<uint32_t>(halfword);
      } else {
        std::println(stderr, "Illegal load instructiion at <0x{:08x}>",
                     (pc - 4));
      }
    } else if (opcode == opcode::store) {
      std::println("store instruction.");
      const uint32_t imm11_5_mask = 0xfe000000;
      const uint32_t rs2_mask = 0x01f00000;
      const uint32_t rs1_mask = 0x000f8000;
      const uint32_t funct3_mask = 0x00007000;
      const uint32_t imm4_0_mask = 0x00000f80;

      const uint32_t imm11_5 = (ins & imm11_5_mask);
      const uint8_t rs2 = (ins & rs2_mask) >> 20;
      const uint8_t rs1 = (ins & rs1_mask) >> 15;
      const uint8_t funct3 = (ins & funct3_mask) >> 12;
      const uint32_t imm4_0 = (ins & imm4_0_mask);

      const uint32_t imm = (imm11_5 >> 20) | (imm4_0 >> 7);
      const int32_t signed_imm = (static_cast<int32_t>(imm) << 20) >> 20;

      const uint32_t target_address =
          (x.at(rs1) + signed_imm) - platform::memory_base;

      if (funct3 == 0b000) { // SB
        const uint8_t b = x.at(rs2) & 0xff;
        memory.at(target_address) = b;
      } else if (funct3 == 0b001) { // SH
        const uint16_t hw = x.at(rs2) & 0xffff;
        const uint8_t b1 = (hw & 0xff00) >> 8;
        const uint8_t b0 = (hw & 0x00ff);
        memory.at(target_address) = b0;
        memory.at(target_address + 1) = b1;
      } else if (funct3 == 0b010) { // SW
        const uint32_t w = x.at(rs2);
        const uint32_t b3 = (w & 0xff000000) >> 24;
        const uint32_t b2 = (w & 0x00ff0000) >> 16;
        const uint32_t b1 = (w & 0x0000ff00) >> 8;
        const uint32_t b0 = (w & 0x000000ff);

        memory.at(target_address) = b0;
        memory.at(target_address + 1) = b1;
        memory.at(target_address + 2) = b2;
        memory.at(target_address + 3) = b3;
      } else {
        std::println(stderr, "Illegal store instruction at <0x{:08x}>",
                     (pc - 4));
      }
    } else if (opcode == opcode::imm_reg) {
      std::println("imm-reg instruction.");
      const uint32_t imm_mask = 0xfff00000;
      const uint32_t rs1_mask = 0x000f8000;
      const uint32_t funct3_mask = 0x00007000;
      const uint32_t rd_mask = 0x00000f80;

      const uint32_t imm = (ins & imm_mask) >> 20;
      const uint8_t rs1 = (ins & rs1_mask) >> 15;
      const uint8_t funct3 = (ins & funct3_mask) >> 12;
      const uint8_t rd = (ins & rd_mask) >> 7;

      const int32_t signed_imm = (static_cast<int32_t>(imm) << 20) >> 20;

      if (funct3 == 0b000) { // ADDI
        x.at(rd) = x.at(rs1) + signed_imm;
      } else if (funct3 == 0b010) { // SLTI
        x.at(rd) = static_cast<int32_t>(x.at(rs1)) < signed_imm;
      } else if (funct3 == 0b011) { // SLTIU
        x.at(rd) = x.at(rs1) < static_cast<uint32_t>(imm);
      } else if (funct3 == 0b100) { // XORI
        x.at(rd) = x.at(rs1) ^ signed_imm;
      } else if (funct3 == 0b110) { // ORI
        x.at(rd) = x.at(rs1) | signed_imm;
      } else if (funct3 == 0b111) { // ANDI
        x.at(rd) = x.at(rs1) & signed_imm;
      } else if (funct3 == 0b001) { // SLLI
        uint8_t shiftamt = imm & 0x1f;
        x.at(rd) = x.at(rs1) << shiftamt;
      } else if (funct3 == 0b101) { // Shift right
        uint8_t shiftamt = imm & 0x1f;
        bool arithmetic = imm & 0b01000000;
        if (arithmetic) { // SRAI
          x.at(rd) = static_cast<int32_t>(x.at(rs1)) >> shiftamt;
        } else { // SRLI
          x.at(rd) = x.at(rs1) >> shiftamt;
        }
      }
    } else if (opcode == opcode::reg_reg) {
      const uint32_t funct7_mask = 0xfe000000;
      const uint32_t rs2_mask = 0x01f00000;
      const uint32_t rs1_mask = 0x000f8000;
      const uint32_t funct3_mask = 0x00007000;
      const uint32_t rd_mask = 0x00000f80;

      const uint8_t funct7 = (ins & funct7_mask) >> 25;
      const uint8_t rs2 = (ins & rs2_mask) >> 20;
      const uint8_t rs1 = (ins & rs1_mask) >> 15;
      const uint8_t funct3 = (ins & funct3_mask) >> 12;
      const uint8_t rd = (ins & rd_mask) >> 7;

      if (funct3 == 0b000) {
        if (funct7 == 0) { // ADD
          x.at(rd) = x.at(rs1) + x.at(rs2);
        } else if (funct7 == 0x40) { // SUB
          x.at(rd) = x.at(rs1) - x.at(rs2);
        }
      } else if (funct3 == 0b001) { // SLL
        const uint8_t shiftamt = x.at(rs2) & 0x1f;
        x.at(rd) = x.at(rs1) << shiftamt;
      } else if (funct3 == 0b010) { // SLT
        x.at(rd) =
            static_cast<int32_t>(x.at(rs1)) < static_cast<int32_t>(x.at(rs2));
      } else if (funct3 == 0b011) { // SLTU
        x.at(rd) = x.at(rs1) < x.at(rs2);
      } else if (funct3 == 0b100) { // XOR
        x.at(rd) = x.at(rs1) ^ x.at(rs2);
      } else if (funct3 == 0b101) {
        const uint8_t shiftamt = x.at(rs2) & 0x1f;
        if (funct7 == 0) { // SRL
          x.at(rd) = x.at(rs1) >> shiftamt;
        } else if (funct7 == 0x40) { // SRA
          x.at(rd) = static_cast<int32_t>(x.at(rs1)) >> shiftamt;
        }
      } else if (funct3 == 0b110) { // OR
        x.at(rd) = x.at(rs1) | x.at(rs2);
      } else if (funct3 == 0b111) { // AND
        x.at(rd) = x.at(rs1) & x.at(rs2);
      }
    } else if (opcode == opcode::fence) {
      std::println("fence instructions unimplemented.");
    } else if (opcode == opcode::system) {
      std::println("system instruction");
      const uint32_t imm_mask = 0xfff00000;
      const uint32_t rs1_mask = 0x000f8000;
      const uint32_t funct3_mask = 0x00007000;
      const uint32_t rd_mask = 0x00000f80;

      const uint32_t imm = (ins & imm_mask) >> 20;
      const uint8_t rs1 = (ins & rs1_mask) >> 15;
      const uint8_t funct3 = (ins & funct3_mask) >> 12;
      const uint8_t rd = (ins & rd_mask) >> 7;

      if (imm == 0) { // ecall
        std::println("ecall is unimplemented, hence halting execution.");
        return;
      }

    } else {
      std::println("unimplemented instruction");
    }
    ins = fetch();
  }
}

}; // namespace riscv
