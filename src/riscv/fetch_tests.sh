#/bin/sh

# fetch tests from https://github.com/riscv-software-src/riscv-tests and build for rv32ui baremetal

RISCV_OBJDUMP="riscv64-unknown-elf-objdump"
RISCV_OBJCOPY="riscv64-unknown-elf-objcopy"
RISCV_HOST="riscv64-unknown-elf"
RISCV_TESTS_DIR="./riscv-tests"
RISCV_TESTS_SIMPLIFIED_DIR="./riscv-tests-simplified"
EMULATOR_EXTENSIONS="m"
FIND_PATTERN="rv32u[mi]-p-*"

set -xe

# if riscv-tests isn't found clone and build
if [ ! -d $RISCV_TESTS_DIR ]; then
	echo "Cloning riscv-tests"
	git clone https://github.com/riscv-software-src/riscv-tests $RISCV_TESTS_DIR
	cd $RISCV_TESTS_DIR
	git submodule update --init --recursive
	echo "Building riscv-tests for rv32uim baremetal"
	autoconf
	./configure --host=$RISCV_HOST CFLAGS="-march=rv32im -mabi=ilp32" --with-xlen=32
	sed -i "s/<sys\/signal\.h>/<signal\.h>/g" "./benchmarks/common/syscalls.c" # doesn't compile without this
	make isa -j$nproc
	cd ..
fi

# if there's no need to git clone, assume already built test executables for rv32ui
mkdir -p $RISCV_TESTS_SIMPLIFIED_DIR
for exe in $(find "$RISCV_TESTS_DIR/isa" -type f -executable -name $FIND_PATTERN)
do
	$RISCV_OBJDUMP -S $exe > "$RISCV_TESTS_SIMPLIFIED_DIR/$(basename $exe).S"
	$RISCV_OBJCOPY -O binary $exe "$RISCV_TESTS_SIMPLIFIED_DIR/$(basename $exe).bin"
done
