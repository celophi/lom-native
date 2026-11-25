#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import zipfile

DESCRIPTION = """Unpack a bug report zip from the launcher and print what I need to start on it.

    python3 tools/triage/open_report.py REPORT.zip [--binary PATH] [--out FOLDER]

It unpacks the zip next to itself (or into --out), then prints the launcher's
summary, the machine from the game's system report, and the crash report if
there is one. With --binary, the game executable from the same release as the
report, it turns the crash's backtrace into function names with
llvm-symbolizer. Release builds keep their debug info, so the names come out
as the decomp's.

The backtrace has offsets from the start of the game's image. ELF (PIE) takes
those as they are. A Windows .exe and a macOS binary are linked to load at an
address of their own, which this adds back on.
"""

SYMBOLIZER = 'llvm-symbolizer'

SYSTEM_SUFFIX = '-system.txt'
CRASH_SUFFIX = '-crash.txt'

SYSTEM_KEYS = ('revision:', 'type:', 'os:', 'cpu:', 'ram:', 'install:', 'session:', 'backend:', 'device:', 'driver version:', 'failed to start:')

FRAME = re.compile(r'^\s+#(\d+)\s+0x([0-9a-f]+)\s+(\S*)\+0x([0-9a-f]+)\s*$')

SYMBOLS_HEADING = 'symbols'

MACHO_TEXT_BASE = 0x100000000

ELF_MAGIC = b'\x7fELF'
ELF_TYPE_EXECUTABLE = 2
PE_MAGIC = b'MZ'
PE_HEADER_OFFSET = 0x3c
PE32_PLUS = 0x20b
MACHO_MAGICS = (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf')


def link_base(binary):
    """What to add to an image offset to get the address llvm-symbolizer wants."""
    data = binary.read_bytes()[:4096]

    if data.startswith(ELF_MAGIC):
        elf_type = struct.unpack_from('<H', data, 16)[0]
        return 0x400000 if elf_type == ELF_TYPE_EXECUTABLE else 0

    if data.startswith(PE_MAGIC):
        header = struct.unpack_from('<I', data, PE_HEADER_OFFSET)[0]
        optional = header + 24
        magic = struct.unpack_from('<H', data, optional)[0]
        return struct.unpack_from('<Q', data, optional + 24)[0] if magic == PE32_PLUS else struct.unpack_from('<I', data, optional + 28)[0]

    if data[:4] in MACHO_MAGICS:
        return MACHO_TEXT_BASE

    raise SystemExit(f'{binary} is not an ELF, PE or Mach-O executable')


def symbolize(frames, binary):
    """Function and source line for each (frame, offset), or None when llvm-symbolizer isn't there."""
    if not shutil.which(SYMBOLIZER):
        print(f'({SYMBOLIZER} not found, so no names)')
        return None

    base = link_base(binary)
    addresses = [hex(base + offset) for _, offset in frames]
    result = subprocess.run([SYMBOLIZER, f'--obj={binary}', '--functions=linkage', '--inlining=false', *addresses],
                            check=True, capture_output=True, text=True)
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    return [(lines[i * 2], lines[i * 2 + 1]) for i in range(len(frames))]


def frame_modules(text):
    """The module each POSIX frame is in, from the "symbols" lines: "lom(+0x1234) [...]" gives "lom"."""
    lines = text.splitlines()
    if SYMBOLS_HEADING not in lines:
        return []
    modules = []
    for line in lines[lines.index(SYMBOLS_HEADING) + 1:]:
        if not line.strip():
            break
        parts = line.split()
        module = parts[1] if parts and parts[0].isdigit() and len(parts) > 1 else line.split('(')[0]
        modules.append(Path(module).name)
    return modules


def game_frames(text):
    """(frame number, offset) for the frames in the game's own image."""
    modules = frame_modules(text)
    frames = []
    for match in map(FRAME.match, text.splitlines()):
        if not match:
            continue
        number = int(match.group(1))
        module = match.group(3) or (modules[number] if number < len(modules) else '')
        if module.lower().startswith('lom'):
            frames.append((number, int(match.group(4), 16)))
    return frames


def print_section(title, text):
    """Print a section of a bug report.

    @param title Its title.
    @param text Its text.
    """
    print(f'== {title} ==')
    print(text.rstrip())
    print()


def main():
    """Unpack a bug report and print what's in it, with function names when the release's executable is given."""
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('report', type=Path, help='the bug report zip')
    parser.add_argument('--binary', type=Path, help="the game executable from the report's release, for function names")
    parser.add_argument('--out', type=Path, help='where to unpack it (default: beside the zip)')
    args = parser.parse_args()

    out = args.out or args.report.with_suffix('')
    with zipfile.ZipFile(args.report) as archive:
        bad = archive.testzip()
        if bad:
            raise SystemExit(f'{args.report} is damaged: {bad} fails its checksum')
        archive.extractall(out)
    print(f'Unpacked into {out}\n')

    summary = out / 'report.txt'
    if summary.exists():
        print_section('Launcher', summary.read_text(errors='replace'))

    for system in sorted((out / 'game').glob(f'*{SYSTEM_SUFFIX}')):
        lines = [line for line in system.read_text(errors='replace').splitlines() if line.startswith(SYSTEM_KEYS)]
        print_section(f'Machine ({system.name})', '\n'.join(lines))

    crashes = sorted((out / 'game').glob(f'*{CRASH_SUFFIX}'))
    if not crashes:
        print('No crash report: the game didn\'t crash in this run, or it was reported from Help.\n')

    for crash in crashes:
        text = crash.read_text(errors='replace')
        print_section(f'Crash ({crash.name})', text)

        frames = game_frames(text)
        if args.binary and frames:
            names = symbolize(frames, args.binary)
            if names:
                print('== Backtrace in the game ==')
                for (frame, _), (function, source) in zip(frames, names):
                    print(f'  #{frame:<3} {function}  {source}')
                print()
        elif frames:
            print('Pass --binary with the game executable from this release to get function names.\n')

    session = out / 'session'
    if session.is_dir():
        end = session / 'end'
        print(f'Session recording in {session}: ended "{end.read_text().strip() if end.exists() else "unknown"}", '
              f'with its inputs.bin and starting memory card.')
    else:
        print('No session recording (it\'s off unless the player turned it on in launcher settings).')


if __name__ == '__main__':
    sys.exit(main())
