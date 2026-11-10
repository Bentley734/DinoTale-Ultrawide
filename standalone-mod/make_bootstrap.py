"""Create the minimal ELF for bootstrap.S without a cross-linker.

The payload is our own two-instruction function, with no game data,
references, replacements or hooks. RecompModTool generates the runtime
symbol format from this normal MIPS ELF32 file.
"""

from pathlib import Path
import argparse
import struct


def make_elf() -> bytes:
    text = struct.pack(">II", 0x03E00008, 0x00000000)  # jr $ra; nop
    strings = b"\0dinotale32x9_bootstrap\0"
    section_strings = b"\0.text\0.symtab\0.strtab\0.shstrtab\0"
    text_offset = 0x100
    sym_offset = text_offset + len(text)
    symbols = bytes(16) + struct.pack(">IIIBBH", 1, 0x80800000, len(text), 0x12, 0, 1)
    str_offset = sym_offset + len(symbols)
    shstr_offset = str_offset + len(strings)
    sh_offset = (shstr_offset + len(section_strings) + 3) & ~3

    # ELF32 MSB, executable MIPS III / O32, one PT_LOAD containing .text.
    ident = b"\x7fELF\x01\x02\x01" + bytes(9)
    header = ident + struct.pack(">HHIIIIIHHHHHH", 2, 8, 1, 0, 52,
                                 sh_offset, 0x20001001, 52, 32, 1, 40, 5, 4)
    program = struct.pack(">IIIIIIII", 1, text_offset, 0x80800000, 0,
                          len(text), len(text), 5, 16)
    result = bytearray(header + program)
    result.extend(bytes(text_offset - len(result)))
    result.extend(text)
    result.extend(symbols)
    result.extend(strings)
    result.extend(section_strings)
    result.extend(bytes(sh_offset - len(result)))

    def section(name: str, kind: int, flags: int, address: int, offset: int,
                size: int, link: int = 0, info: int = 0, alignment: int = 1,
                entry_size: int = 0) -> bytes:
        name_offset = section_strings.index(name.encode() + b"\0") if name else 0
        return struct.pack(">IIIIIIIIII", name_offset, kind, flags, address,
                           offset, size, link, info, alignment, entry_size)

    result.extend(bytes(40))
    result.extend(section(".text", 1, 6, 0x80800000, text_offset, len(text), alignment=16))
    result.extend(section(".symtab", 2, 0, 0, sym_offset, len(symbols), link=3,
                          info=1, alignment=4, entry_size=16))
    result.extend(section(".strtab", 3, 0, 0, str_offset, len(strings)))
    result.extend(section(".shstrtab", 3, 0, 0, shstr_offset, len(section_strings)))
    return bytes(result)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(make_elf())
    print(f"Created {args.output}")
