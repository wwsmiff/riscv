#include <cstdint>
#include <print>

namespace riscv {
namespace platform {
constexpr auto ialign{32};
constexpr auto ilen{32};
} // namespace platform
}; // namespace riscv

int main() {
  std::println("Hello, RISC-V!");
  return 0;
}
