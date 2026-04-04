#include <cstdint>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <vector>

namespace riscv {
namespace platform {

using opcode_t = uint8_t;
using instruction_t = uint32_t;

constexpr auto ialign{32};
constexpr auto ilen{32};
constexpr auto allow_misaligned_accesses{false};
constexpr auto memory_size{2 << 20}; // 2 MiB.
} // namespace platform

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

} // namespace op
/* clang-format on */

struct Core {
  uint32_t pc{};
  std::array<uint32_t, 32> x{};
  std::array<uint8_t, platform::memory_size> memory{};

  void load_binfile(const std::vector<uint8_t> &bindata) {
    std::copy(bindata.cbegin(), bindata.cend(), memory.begin());
    std::println("{}/{} bytes used.", bindata.size(), platform::memory_size);
    memory.at(bindata.size()) = 0xff;
    memory.at(bindata.size() + 1) = 0xff;
    memory.at(bindata.size() + 2) = 0xff;
    memory.at(bindata.size() + 3) = 0xff;
  }

  platform::instruction_t fetch() {
    platform::instruction_t ins{};

    const uint8_t byte0 = memory.at(pc);
    const uint8_t byte1 = memory.at(pc + 1);
    const uint8_t byte2 = memory.at(pc + 2);
    const uint8_t byte3 = memory.at(pc + 3);

    const uint16_t parcel0 = static_cast<uint16_t>((byte1 << 8) | byte0);
    const uint16_t parcel1 = static_cast<uint16_t>((byte3 << 8) | byte2);

    ins = static_cast<platform::instruction_t>((parcel1 << 16) | parcel0);
    pc += 4;

    return ins;
  }

  void execute() {
    platform::instruction_t ins{};
    while (ins != 0xffffffff) {
      ins = fetch();
      std::print("0x{:08x}: ", ins);
      const uint32_t opcode_mask = 0x7f;
      const uint8_t opcode = ins & opcode_mask;

      if (opcode == opcode::lui) { // U-type
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
          std::println(
              stderr, "Target address for JAL<0x{:08x}> is not 4-byte aligned.",
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
          std::println(stderr, "Illegal instruction at <0x{:08x}>", (pc - 4));
          return;
        }
        const int32_t offset = (static_cast<int32_t>(imm) << 20) >> 20;
        const int32_t target_address = (x.at(rs1) + offset) & ~1;
        if ((target_address & 0x3) != 0) {
          std::println(
              stderr,
              "Target address for JALR<0x{:08x}> is not 4-byte aligned.",
              (pc - 4));

          return;
        }
        if (target_address == 0) {
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
        const uint32_t imm4_1_mask = 0x00000f80;
        const uint32_t imm11_mask = 0x00000040;

        const uint8_t rs1 = (ins & rs1_mask) >> 15;
        const uint8_t rs2 = (ins & rs2_mask) >> 20;
        const uint8_t funct3 = (ins & funct3_mask) >> 12;

        const uint32_t imm12 = ins & imm12_mask;
        const uint32_t imm10_5 = ins & imm10_5_mask;
        const uint32_t imm4_1 = ins & imm4_1_mask;
        const uint32_t imm11 = ins & imm11_mask;

        const uint32_t imm =
            (imm12 >> 19) | (imm11 << 5) | (imm10_5 >> 20) | (imm4_1 >> 7);

        int32_t offset = (static_cast<int32_t>(imm) << 19) >> 19;
        offset <<= 1;

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
          if (static_cast<int32_t>(x.at(rs1)) <
              static_cast<int32_t>(x.at(rs2))) {
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
          std::println(stderr, "Illegal instruction at <0x{:08x}>", (pc - 4));
        }
      } else if (opcode == opcode::load) {
        std::println("load instruction.");
      } else if (opcode == opcode::store) {
        std::println("store instruction.");
      } else if (opcode == opcode::imm_reg) {
        std::println("imm-reg instruction.");
      } else if (opcode == opcode::reg_reg) {
        std::println("reg-reg instruction.");
      } else if (opcode == opcode::fence) {
        std::println("fence instruction.");
      } else if (opcode == opcode::system) {
        std::println("system instruction.");
      }
    }
  }
};
}; // namespace riscv

std::vector<std::string> cmdargs(int argc, char **argv) {
  argc--;
  argv++;
  std::vector<std::string> args{};
  for (int i{}; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  return args;
}

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

int main(int argc, char **argv) {
  const auto args = cmdargs(argc, argv);
  if (args.size() == 0) {
    std::println(stderr, "Must specify a binary file.");
    return EXIT_FAILURE;
  }

  const auto exepath = args.at(0);
  if (!std::filesystem::exists(exepath)) {
    return EXIT_FAILURE;
  }

  const auto exedata = bindata(exepath);

  riscv::Core core{};
  core.load_binfile(exedata);
  core.execute();

  for (size_t i{}; i < core.x.size(); ++i) {
    auto val = core.x.at(i);
    std::println("x{} = 0x{:08x}, 0b{:032b}, {}", i, val, val, val);
  }

  return 0;
}
