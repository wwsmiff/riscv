#include <cstdint>
#include <print>
#include <string>
#include <vector>

#include "core.hpp"
#include "platform.hpp"

std::vector<std::string> cmdargs(int argc, char **argv) {
  argc--;
  argv++;
  std::vector<std::string> args{};
  for (int i{}; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  return args;
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

  auto exedata = riscv::bindata(exepath);

  riscv::Core core{};
  core.x.at(2) = riscv::platform::memory_size;

  core.load_bindata(exedata);
  core.run();

  if constexpr (riscv::platform::verbose) {
    for (size_t i{}; i < core.x.size(); ++i) {
      auto val = core.x.at(i);
      std::println("x{} = 0x{:08x}, 0b{:032b}, {}", i, val, val, val);
    }
  }

  // for riscv-tests [https://github.com/riscv-software-src/riscv-tests]

  // eg.
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
