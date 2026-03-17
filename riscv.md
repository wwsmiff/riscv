# This document is for tracking my understanding of the RISC-V instruction set as I progress through its [instruction set manual](https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html) and implement a simple emulator.

### core
A `core` is anything which has an independent instruction fetching unit.

### Execution Environment Interface
This defines the initial state of the program and other specific hardware details like number of `harts`, privilege modes supported, attributes of memory and I/O regions, etc. _The RISC-V ISA is a part of the `EEI`_.

So essentially, `EEIs` are the base of any RISC-V platform, be it hardware or software, and the `EEI` consists of everything from the definition of how interrupts must be handled to memory details and so on.

### hart
A `hart` is simply a unit that fetches and executes a RISC-V instruction within an `EEI`, so essentially a hardware thread.

### ISAs
A RISC-V ISA is the base integer ISA which every implementation must have and any other extensions to the base. There are 4 base integer variants (RV32I, RV64I, RV32E, RV64I). Initially, this project will focus on implementing RV32I. _RV32I is a distinct ISA from RV64I and there is no intention to support backwards compatability or anything similar._

### ISA Prefixes
- `I`: Base integer operations.
- `M`: Integer multiplication and division extension.
- `A`: Adds instructions that supports atomic operations.
- `F`: Adds support for single-precision floating point operations.
- `D`: Adds support for double-precision floating point operations.
- `C`: Compressed instruction set which provides narrower 16-bit forms of common instructions.

### Memory
A `hart` has a single byte-addressable address space of `2^XLEN`` bytes for all memory accesses.
- `word` - 32 bits (4 bytes).
- `doubleword` - 64 bits (8 bytes).
- `halfword` - 16 bits (2 bytes).
- The memory address space is circular, so the byte at address `2^XLEN - 1` is adjacent to the byte at address 0.
- Memory address computations done by hardware ignore overflow and wrap around to modulo `2^XLEN`.
The `EEI` determines the mapping of the resources onto a `hart`'s address space.
- Multiple `hart`s can share the same address space
- Executing RISC-V instructions entails one or more `implicit` or `explicit` memory accesses. For each instruction an `implicit`access is done to fetch instructions. Most RISC-V instructions do not perform further memory accesses apart from implicitly fetching instructions. Other such `implicit` memory accesses may be defined by the `EEI`.
- load and store instructions perform `explicit` memory accessess _(obviously)_.
- Instructions that try to access inaccessible memory will raise an exception.
- If unspecified, implicit reads that have no side effects may occur arbitrarily, to ensure implicit reads only occur after writes to the same memory space, fencing or cache-control instructions must be executed.
- The perception of memory accesses between multiple `hart`s may change because the default consistency model is `RISC V Weak Memory Order` (RVWMO). This can be changed by the `EEI`.

### Base instruction encoding
- The base ISA has fixed length instructions of *32-bits* that must be naturally aligned on 32-bit boundaries.
- The RISC-V encoding scheme is best designed to support variable legnth instructions where each instruction consists of 16-bit instruction parcels and are naturally aligned on 16-bit boundaries.
- `IALIGN` - defines the instruction address alignment constraint which the implementation enforces.
- `ILEN` - defines the maximum length (in bits) of an instruction, which is always a multiple of `IALIGN`.
- All instructions in 32-bit base ISA have their lowest two bits set to `11`.
- Instructions with the lower 16 bits [15:0] as all zeroes are illegal instructions eg. `0xabcd0000` and are considered to be of minimal length. Instructions with bits [ILEN-1:0] -> [31:0] all zeroes is also illegal. This instruction is considered to be ILEN bits long. _This is to catch unintented jumps to zeroed out memory regions._ Similarly, instructions containing all 1s are also illegal to catch other patterns when interacting with unprogrammed non-volatile memory devices, non-functional memory buses, etc.
- Instructions are stored in memory as 16-bit _little-endian_ `parcels`. with lowest-addressed `parcel`s holding lowest numbered bits.
```
| 0xabcd | 0xwxyz | -> one instruction (two parcels)
    h        l
```
- This design is to allow the instruction fetching unit to access the length-encoding bits first and examining the first few bits, when dealing with variable-length instructions. _The `parcel`s themselves are also little-endian._
### Exceptions, interrupts and traps
- `exceptions` - Unusual conditions occuring at runtime
- `interrupt` - External asynchronous event causing a hart to suddenly change the flow of the execution.
- `trap` - Refers to a transfer of control to a `trap handler` due to exceptions or interrupts.

### RV32I
- The general purpose registers are named `x` and there are 32 such registers of 32-bits each.
- `x0` is hardwired to all bits equal to 0.
- There is only one other unprivileged register, the program counter `pc`.
- There is no dedicated stack pointer and according to calling convention `x1` holds the return address, `x2` is the stack pointer and `x5` is an alternate link register.
- Immediates are always sign-extended and are packed towards the leftmost available bits of the instruction. The sign bit is always in bit 31.
- There are 4 kinds of base instructions `R`/`I`/`S`/`U`.
**R Instructions**
```
[31:25] - funct7
[24:20] - rs2
[19:15] - rs1
[14:12] - funct3
[11:7]  - rd
[6:0]   - opcode
```

**I Instructions**
```
[31:20] - imm[11:0]
[19:15] - rs1
[14:12] - funct3
[11:7]  - rd
[6:0]   - opcode
```

**S Instructions**
```
[31:25] - imm[11:5]
[24:20] - rs2
[19:15] - rs1
[14:12] - funct3
[11:7]  - imm[4:0]
[6:0]   - opcode
```

**U Instructions**
```
[31:12] - imm[31:12]
[11:7]  - rd
[6:0]   - opcode
```

There are two variations of the instruction format `B` and `J` based on handling of immediates.

**B Instructions**
```
[31]    - imm[12]
[30:25] - imm[10:5]
[24:20] - rs2
[19:15] - rs1
[14:12] - funct3
[11:8]  - imm[4:1]
[7]     - imm[11]
[6:0]   - opcode
```
In `B` instrutions, the 12-bit immediate field is used to encode branch offets in multiples of 2. Instead of shifting all bits in the instruction-encoded immmediate left by one in hardware, the middle bits [10:1] and sign bit stay in fixed positions while the lowest bit in the `S` instructions encodes a higher-order bit in the `B` format.

**J Instructions**
```
[31]    - imm[20]
[30:21] - imm[10:1]
[20]    - imm[11]
[19:12] - imm [19:12]
[11:7]  - rd
[6:0]   - opcode
```

In `U` instructions, the 20-bit immediate is shifted left by 12 bits to form U immediates and by 1 bit to form `J` immediates.

Notably, the first source register `rs1`, second source register `rs2` and destination register `rd` all maintain the same physical mapping within the instrucion to simplify decoding.

### Integer computation instructions
- Operate on instructions of `XLEN` bits
- Encoded as `register-immediate` using `I` instructions or `register-register` with `R` instructions.
- There are no special instructions for overflow checking.

#### Integer register-immediate instructions - 23 instructions
###### Arithmetic instructions.
- `ADDI`, `SLTI`, `ANDI`, `ORI`, `XORI`
- `ADDI` - adds sign-extended 12 bit immediate to `rs1`. Overflow is ignored and result is the low `XLEN` bits of the result. _`ADDI` rd, rs1, 0` is used to implement the pseudoinstruction `MV rd1, rs1`.

###### Comparison instruction
- `SLTI` - places `1` in `rd` if `rs1` is less than the sign-extended immediate when both are signed numbers else `0`. For unsigned integers, `SLTIU` is used, it sets `rd` to `1` is `rs1` equals zero else  `rd` is set to `0`.

###### Logical instructions
- `ANDI/ORI/XORI` - logical operations AND, OR and XOR respectively on `rs1` and signed extended 12-bit immediate and place result in `rd`. _`XORI rd, rs1, -1` performs bitwise logical inversion of rs1_.

###### Shift instructions
- `SLLI/SRLI/SRAI` - Shifts by a constant. The shift amount is encoded in the lower 5 bits. The right-shift type is encoded in bit 30, and `SRAI` is an arithemetic shift (the original sign bit is copied onto the vacated upper bits).

###### Misc (??)
- `LUI` - Load upper immediate, used to build 32-bit constants and uses the `U` format. LUI places the 32-bit `U-immediate` value into `rd`, filling in the lowest 12 bits with 0s.
- `AUIPC` - Add upper immediate to `pc`, used to build pc-relative addresses, uses the `U` format and places the result in `rd`.

#### Integer resgister-register instructions
###### Arithmetic instructions
`ADD/SLT` - Same as the corresponding `register-immediate` instructions.
`AND/OR/XOR` - Perform logical bitwise operations.
`SLL/SRL/SRA` - Perform logical shl, logical shr and arithmetic shift on `rs2` by the amount held in the lower 5 bits of `rs2`.

### NOP - 1 instruction
- `nop` - Advances `pc` without any changes to the current state. Encoded as `ADDI x0, x0, 0`. _It can also be used to align code segments_.

### Control Transfer Instructions - 6 instructions
There are two kinds of branching instructions, conditional and unconditional. _If an instruction causes which is the target of a jump raises an exception, the target instruction is reported, not the branch_

###### Unconditional branching
- `JAL` - Jump and Link. uses the `J` format and encodes a signed offset in multiples of 2. The offset is signed extended and added to the address of the jump instruction to form the target address. Jumps can target a + or - 1MB range. The address of the instruction following the jump (`pc` + 4) is stored into `rd`. According to conventions, `x1` is used to store the return address.
- `JALR` - Uses the `I` format and is an indirect jump. The offset is calculated by adding the signed-extended 12-bit immediate to `rs1`. The next instruction address after the jump instruction will be stored in `rd`. _It is important to clear the LSB_.

###### Conditional branching
All branch instructions use the `B` format. The 12-bit immediate encodes signed offsets in multiples of 2 bytes, conditional branch range is `+` or `-` 4 KB.
- `BEQ/BNE` - Branch if equal/not equal. `rs1 == rs2`.
- `BLT[U]` - Branch if lesser than. `rs1 < rs2`.
- `BGE[U]` - Branch if greater than or equal to. `rs1 >= rs2`.

### Load and store instructions
Loads are encoded in `I` format and stores are encoded in `S` format. The effective address is obtained by adding `rs1` to the sign-extended 12-bit offset.
###### Load instructions
- Loads copy a value from memory to `rd`.
- `LW` - loads a 32-bit value from memory into `rd`.
- `LH` - loads 16-bit value from memory and sign-extends to 32-bits and stores in `rd`.
- `LHU` - loads 16-bit value from memory and zero extends to 32-bits and stores in `rd`.
- `LB[U]` - defined for 8-bit values.
###### Store instructions
- Stores copy the value in `rs2` to memory.
- `SW` - store 32-bits from `rs2` to memory.
- `SH` - store low 16-bits from `rs2` to memory.
- `SB` - store low 8-bits from `rs2` to memory.

### Memory ordering instructions
... yet to implement
