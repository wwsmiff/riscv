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
constexpr opcode_t store	 = 0b0'0000011;

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

struct Executable {};

struct Core {
  uint32_t pc{};
  std::array<uint32_t, 32> x{};
  std::array<uint8_t, platform::memory_size> memory{};
  void load_binfile(const std::vector<uint8_t> &bindata) {
    std::copy(bindata.cbegin(), bindata.cend(), memory.begin());
    std::println("{}/{} bytes used.", bindata.size(), platform::memory_size);
  }

  platform::instruction_t fetch() {
    platform::instruction_t ins{};

    uint8_t byte0 = memory.at(pc);
    uint8_t byte1 = memory.at(pc + 1);
    uint8_t byte2 = memory.at(pc + 2);
    uint8_t byte3 = memory.at(pc + 3);

    uint16_t parcel0 = static_cast<uint16_t>((byte1 << 8) | byte0);
    uint16_t parcel1 = static_cast<uint16_t>((byte3 << 8) | byte2);

    ins = static_cast<platform::instruction_t>((parcel1 << 16) | parcel0);
    pc += 4;

    return ins;
  }

  void execute() {
    platform::instruction_t ins = fetch();
    std::println("Decoded instruction: {:08x}h", ins);
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
  core.execute();

  return 0;
}
