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

  return 0;
}
