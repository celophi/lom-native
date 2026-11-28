#!/usr/bin/env python3

import struct
import zlib
from pathlib import Path

OUTPUT = Path(__file__).resolve().parents[2] / 'tests/support/platform/remaster_fixture.h'


def int_array(values):
    """PSB int array: type 0x0D+n-1 with an n-byte count, then the item size's type, then the items."""
    values = list(values)
    size = max(1, max((v.bit_length() + 7) // 8 for v in values) if values else 1)
    count = len(values)
    count_size = max(1, (count.bit_length() + 7) // 8)
    out = bytes([0x0C + count_size]) + count.to_bytes(count_size, 'little') + bytes([0x0C + size])
    return out + b''.join(v.to_bytes(size, 'little') for v in values)


def names_trie(names):
    """The key names as FreeMote's trie: charset (a base per node), tree (each node's parent) and, per name, the
    node that ends it (its parent is the name's last character). A node's id is its parent's base plus its
    character, the end of a name being character 0."""
    children = [{}]
    ends = []
    for name in names:
        node = 0
        for ch in name.encode() + b'\0':
            if ch not in children[node]:
                children.append({})
                children[node][ch] = len(children) - 1
            node = children[node][ch]
        ends.append(node)
    ids, used, base, order = {0: 0}, {0}, {}, [0]
    while order:
        node = order.pop(0)
        kids = children[node]
        if not kids:
            continue
        b = 1
        while any(b + ch in used for ch in kids):
            b += 1
        base[node] = b
        for ch, kid in sorted(kids.items()):
            ids[kid] = b + ch
            used.add(b + ch)
            order.append(kid)
    size = max(used) + 1
    charset, tree = [0] * size, [0] * size
    parent_of = {kid: node for node, kids in enumerate(children) for kid in kids.values()}
    for node, nid in ids.items():
        charset[nid] = base.get(node, 0)
        if node:
            tree[nid] = ids[parent_of[node]]
    return int_array(charset) + int_array(tree) + int_array([ids[e] for e in ends])


class Psb:
    def __init__(self):
        """Start an empty PSB."""
        self.names, self.strings, self.chunks = [], [], []

    def name(self, text):
        """Add a name to the PSB's names, once.

        @param text The name.
        @return Its number.
        """
        if text not in self.names:
            self.names.append(text)
        return self.names.index(text)

    def value(self, v):
        """Encode a value as a PSB value.

        @param v A bool, number, string, bytes, list or dict.
        @return Its bytes.
        """
        if isinstance(v, bool):
            return bytes([3 if v else 2])
        if isinstance(v, int):
            n = max(1, (v.bit_length() + 8) // 8)
            return bytes([4 + n]) + v.to_bytes(n, 'little', signed=True)
        if isinstance(v, str):
            if v not in self.strings:
                self.strings.append(v)
            return bytes([0x15]) + bytes([self.strings.index(v)])
        if isinstance(v, bytes):
            self.chunks.append(v)
            return bytes([0x19]) + bytes([len(self.chunks) - 1])
        if isinstance(v, list):
            items = [self.value(x) for x in v]
            offsets, at = [], 0
            for item in items:
                offsets.append(at)
                at += len(item)
            return b'\x20' + int_array(offsets) + b''.join(items)
        if isinstance(v, dict):
            keys = sorted(v, key=self.name)
            names = [self.name(k) for k in keys]
            items = [self.value(v[k]) for k in keys]
            offsets, at = [], 0
            for item in items:
                offsets.append(at)
                at += len(item)
            return b'\x21' + int_array(names) + int_array(offsets) + b''.join(items)
        raise TypeError(v)

    def build(self, root):
        """Build a whole PSB from a tree of values.

        @param root The root object.
        @return The PSB's bytes.
        """
        for key in collect_keys(root):
            self.name(key)
        body = self.value(root)
        names = names_trie(self.names)
        strings_data = b''.join(s.encode() + b'\0' for s in self.strings)
        string_offsets, at = [], 0
        for s in self.strings:
            string_offsets.append(at)
            at += len(s.encode()) + 1
        chunk_offsets, at = [], 0
        for c in self.chunks:
            chunk_offsets.append(at)
            at += len(c)
        header = 40
        names_at = header
        strings_at = names_at + len(names)
        strings_offsets = int_array(string_offsets)
        strings_data_at = strings_at + len(strings_offsets)
        root_at = strings_data_at + len(strings_data)
        chunk_offsets_bytes = int_array(chunk_offsets)
        chunk_lengths_bytes = int_array([len(c) for c in self.chunks])
        chunk_offsets_at = root_at + len(body)
        chunk_lengths_at = chunk_offsets_at + len(chunk_offsets_bytes)
        chunk_data_at = chunk_lengths_at + len(chunk_lengths_bytes)
        head = b'PSB\0' + struct.pack('<HHI', 2, 0, header) + struct.pack('<7I', names_at, strings_at, strings_data_at, chunk_offsets_at,
                                                                          chunk_lengths_at, chunk_data_at, root_at)
        return head + names + strings_offsets + strings_data + body + chunk_offsets_bytes + chunk_lengths_bytes + b''.join(self.chunks)


def collect_keys(value):
    """Every key in a tree of dicts and lists.

    @param value The tree.
    @return The keys, in order.
    """
    if isinstance(value, dict):
        for key, item in value.items():
            yield key
            yield from collect_keys(item)
    elif isinstance(value, list):
        for item in value:
            yield from collect_keys(item)


def c_array(name, data):
    """Write bytes as a C array.

    @param name The array's name.
    @param data The bytes.
    @return The array's text.
    """
    lines = [f'static const unsigned char {name}[] = {{']
    for i in range(0, len(data), 16):
        lines.append('    ' + ', '.join(f'0x{b:02x}' for b in data[i:i + 16]) + ',')
    lines.append('};')
    return '\n'.join(lines)


def main():
    """Write the Remaster file fixtures: a PSB, the same packed as mdf, zlib streams of each block kind, and a WMA
    sound archive like a movie's track."""
    pcm = bytes(range(256)) * 4
    root = {'id': 'sound_archive', 'voice': {'music_voice': {'channelList': [{'archData': {
        'fmt': struct.pack('<HHIIHH', 1, 2, 44100, 176400, 4, 16), 'data': pcm, 'loop': [16, 200], 'wav': 'music_original/music002.wav'}}],
        'loop': 2, 'loopstr': 'all'}}, 'negative': -300}
    psb = Psb().build(root)
    wma = {'id': 'sound_archive', 'voice': {'music_voice': {'channelList': [{'archData': {
        'fmt': struct.pack('<HHIIHHH', 0x161, 2, 44100, 12000, 4459, 16, 0), 'dpds': struct.pack('<2I', 4096, 8192),
        'data': bytes(range(255)) * 2, 'wav': 'music_0013.wav'}}], 'loop': 0, 'loopstr': 'none'}}}
    xwma = Psb().build(wma)
    text = (b'Legend of Mana ' * 40) + bytes(range(256))
    streams = [zlib.compress(text, 0), zlib.compress(text[:40], 9), zlib.compress(text, 9)]
    packed = b'mdf\0' + struct.pack('<I', len(psb)) + zlib.compress(psb, 9)
    parts = ['/* Generated by tools/testing/make_psb_fixture.py. */', '// clang-format off',
             c_array('fixture_psb', psb), c_array('fixture_mdf', packed), c_array('fixture_text', text),
             c_array('fixture_stored', streams[0]), c_array('fixture_fixed', streams[1]), c_array('fixture_dynamic', streams[2]),
             c_array('fixture_xwma_psb', xwma), f'#define FIXTURE_PCM_BYTES {len(pcm)}', '']
    OUTPUT.write_text('\n'.join(parts))
    print(f'wrote {OUTPUT}')


if __name__ == '__main__':
    main()
