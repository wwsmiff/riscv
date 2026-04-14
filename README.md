### for my understanding of riscv, check [this](riscv.md) out
### To build and run [tests](https://github.com/riscv-software-src/riscv-tests):
```
git clone https://github.com/wwsmiff/riscv
cd riscv
make
chmod +x fetch_tests.sh
./fetch_tests.sh
chmod +x run_tests.sh
./run_tests.sh
```

currently all tests for `rv32ui-p-*` pass. `ui` - user-level, integer, `p` - no virtual memory, single core.
