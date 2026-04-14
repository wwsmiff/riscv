#include <cstdint>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <vector>

#include "riscv.hpp"

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
  core.x.at(2) = riscv::platform::memory_size;

  core.load_binfile(exedata);
  core.execute();

  for (size_t i{}; i < core.x.size(); ++i) {
    auto val = core.x.at(i);
    std::println("x{} = 0x{:08x}, 0b{:032b}, {}", i, val, val, val);
  }

  // for riscv-tests [https://github.com/riscv-software-src/riscv-tests]

  // pass:
  // 0ff0000f          	fence
  // 00100193          	li	gp,1
  // 05d00893          	li	a7,93
  // 00000513          	li	a0,0  <--- line of interest
  // 00000073          	ecall

  // fail:
  // 0ff0000f          	fence
  // 00018063          	beqz	gp,80000650 <fail+0x4>
  // 00119193          	slli	gp,gp,0x1
  // 0011e193          	ori	gp,gp,1
  // 05d00893          	li	a7,93
  // 00018513          	mv	a0,gp <--- line of interest
  // 00000073          	ecall

  // since there is no other way of testing
  // whether a test passed or failed for now,
  // the above lines are used to check for the same
  // if the test passes a0 == 0 else a0 != 0
  // a0 is x10

  if (core.x.at(10) != 0) {
    std::println("tests failed.");
  } else {
    std::println("tests passed.");
  }

  return 0;
}
