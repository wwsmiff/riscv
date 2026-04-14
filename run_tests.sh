#!/bin/sh

RISCV_TESTS_SIMPLIFIED_DIR="./riscv-tests-simplified"

for exe in $(find "$RISCV_TESTS_SIMPLIFIED_DIR" -type f -executable -name "rv32ui-p-*")
do
	echo -n "$exe: " && ./main $exe | grep -e "pass" -e "fail"
done
