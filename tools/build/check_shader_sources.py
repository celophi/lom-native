#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path

DESCRIPTION = """Check the checked-in shader headers were built from the shaders as they are now.

    python3 tools/build/check_shader_sources.py

The compiled shaders are checked in as headers in src/gpu/shaders, so a normal
build doesn't need a shader compiler. That means nothing notices when a shader
changes and its headers don't. compile_renderer_shaders.py and
compile_presentation_shaders.py each write a list of the sources they built
from, with a SHA-256 of each, next to the headers. This compares those lists
with the sources, and stops with an error naming each one that changed since.
It needs no shader compiler, so it runs as part of every build.

Line endings don't count, so a checkout with CRLF passes too.
"""

REPO = Path(__file__).resolve().parents[2]
SHADER_DIR = REPO / 'src/gpu/shaders'

RENDERER_STAMP = SHADER_DIR / 'renderer_shaders.sha256'
RENDERER_SOURCES = ('psx.vert.glsl', 'psx.frag.glsl')

PRESENTATION_STAMP = SHADER_DIR / 'presentation_shaders.sha256'
PRESENTATION_SOURCES = ('xbr.vert.glsl', 'xbr.frag.glsl', 'xbr.vert.hlsl', 'xbr.frag.hlsl', 'xbr.vert.metal', 'xbr.frag.metal')

STAMPS = {
    RENDERER_STAMP: ('compile_renderer_shaders.py', RENDERER_SOURCES),
    PRESENTATION_STAMP: ('compile_presentation_shaders.py', PRESENTATION_SOURCES),
}


def source_digest(name):
    """Hash one shader source, ignoring line endings.

    @param name The source's file name in src/gpu/shaders.
    @return The SHA-256 of its text with CRLF read as LF, in hex.
    """
    data = (SHADER_DIR / name).read_bytes().replace(b'\r\n', b'\n')
    return hashlib.sha256(data).hexdigest()


def stamp_text(sources):
    """The list a generator writes after building its headers.

    @param sources The source file names it builds from.
    @return One line per source, in sha256sum's format, with its path from the repo's root.
    """
    return ''.join(f'{source_digest(name)}  {(SHADER_DIR / name).relative_to(REPO).as_posix()}\n' for name in sources)


def write_stamp(stamp):
    """Record the sources a generator just built its headers from.

    @param stamp RENDERER_STAMP or PRESENTATION_STAMP.
    """
    stamp.write_bytes(stamp_text(STAMPS[stamp][1]).encode())


def changed_sources(stamp):
    """List the sources that changed since a generator last wrote its headers.

    @param stamp RENDERER_STAMP or PRESENTATION_STAMP.
    @return The changed sources' file names; empty when the headers are up to date.
    """
    recorded = {}
    if stamp.exists():
        for line in stamp.read_text().splitlines():
            digest, _, path = line.partition('  ')
            recorded[Path(path).name] = digest
    return [name for name in STAMPS[stamp][1] if recorded.get(name) != source_digest(name)]


def main():
    """Check every generator's list, and stop with an error naming what to rerun."""
    argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter).parse_args()
    problems = []
    for stamp, (generator, _) in STAMPS.items():
        changed = changed_sources(stamp)
        if changed:
            problems.append(f'{", ".join(changed)} changed since the headers were built; run tools/build/{generator}')
    if problems:
        raise SystemExit('\n'.join(problems))
    print('shader headers match their sources')


if __name__ == '__main__':
    main()
