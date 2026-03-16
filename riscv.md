# This document is for tracking my understanding of the RISC-V instruction set as I progress through its (instruction set manual)[https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html] and implement a simple emulator.

### core
A `core` is anything which has an independent instruction fetching unit.

### Execution Environment Interface
This defines the initial state of the program and other specific hardware details like number of `harts`, privilege modes supported, attributes of memory and I/O regions, etc. *The RISC-V ISA is a part of the `EEI`*.

So essentially, `EEIs` are the base of any RISC-V platform, be it hardware or software, and the EEI consists of everything from the definition of how interrupts must be handled to memory details and so on.

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
| 0xabcd | 0xwxyz | -> one instruction (two `parcel`s)
    h        l
- This design is to allow the instruction fetching unit to access the length-encoding bits first and examining the first few bits, when dealing with variable-length instructions. _The `parcel`s themselves are also little-endian._
### Exceptions, interrupts and traps
- `exceptions` - Unusual conditions occuring at runtime
- `interrupt` - External asynchronous event causing a hart to suddenly change the flow of the execution.
- `trap` - Refers to a transfer of control to a `trap handler` due to exceptions or interrupts.
