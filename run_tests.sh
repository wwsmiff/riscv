#!/bin/sh

RISCV_TESTS_SIMPLIFIED_DIR="./riscv-tests-simplified"
EMULATOR_EXTENSIONS="m"
FIND_PATTERN="rv32u[mi]-p-*"

if [ $EMULATOR_EXTENSIONS != "m" ]; then
	FIND_PATTERN="rv32ui-p-*"
fi

for exe in $(find "$RISCV_TESTS_SIMPLIFIED_DIR" -type f -executable -name "$FIND_PATTERN")
do
	echo -n "$exe: " && ./main $exe | grep -e "pass" -e "fail"
done
