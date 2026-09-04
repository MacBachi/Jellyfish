#!/usr/bin/env python3
"""Print how much flash and RAM a JellyFloatOS build takes, from its ELF file.

No toolchain needed: the section headers are read directly. Flash is everything the
loader copies from flash (code, constants, initialised data); static RAM is the
initialised plus zero-initialised data. Heap and the two core stacks come on top of
the static RAM figure at run time.

    python3 firmware_cpp/tools/elf_size.py firmware_cpp/build/JellyFloatOS.elf
"""
import struct
import sys

SHT_NOBITS = 8
SHF_WRITE = 0x1
SHF_ALLOC = 0x2

FLASH_BYTES = 4 * 1024 * 1024  # Pico 2 W
RAM_BYTES = 520 * 1024         # RP2350 SRAM


def sections(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"\x7fELF":
        raise SystemExit(f"{path}: not an ELF file")
    is64 = data[4] == 2
    little = data[5] == 1
    e = "<" if little else ">"
    if is64:
        shoff, = struct.unpack_from(e + "Q", data, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from(e + "HHH", data, 0x3A)
    else:
        shoff, = struct.unpack_from(e + "I", data, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from(e + "HHH", data, 0x2E)
    raw = []
    for i in range(shnum):
        off = shoff + i * shentsize
        if is64:
            name, typ, flags, addr, offset, size = struct.unpack_from(e + "IIQQQQ", data, off)
        else:
            name, typ, flags, addr, offset, size = struct.unpack_from(e + "IIIIII", data, off)
        raw.append((name, typ, flags, addr, offset, size))
    strtab_off = raw[shstrndx][4]
    out = []
    for name, typ, flags, addr, offset, size in raw:
        end = data.index(b"\0", strtab_off + name)
        out.append((data[strtab_off + name:end].decode(), typ, flags, addr, size))
    return out


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    flash = 0
    ram = 0
    rows = []
    for name, typ, flags, addr, size in sections(sys.argv[1]):
        if not flags & SHF_ALLOC or size == 0:
            continue
        in_flash = typ != SHT_NOBITS
        in_ram = bool(flags & SHF_WRITE) or (0x20000000 <= addr < 0x20100000)
        if in_flash:
            flash += size
        if in_ram:
            ram += size
        rows.append((name, size, "flash" if in_flash else "", "ram" if in_ram else ""))
    for name, size, f, r in rows:
        print(f"  {name:<20} {size:>8}  {f:<5} {r}")
    print()
    print(f"flash: {flash:>8} bytes  {flash / 1024:7.1f} KB  {100 * flash / FLASH_BYTES:5.1f} % of {FLASH_BYTES // 1024} KB")
    print(f"ram:   {ram:>8} bytes  {ram / 1024:7.1f} KB  {100 * ram / RAM_BYTES:5.1f} % of {RAM_BYTES // 1024} KB (static; heap and stacks come on top)")


if __name__ == "__main__":
    main()
