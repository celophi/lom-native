#!/usr/bin/env python3
import argparse
import re
import struct
import sys
from pathlib import Path

DESCRIPTION = """Make src/mods/japanese_balance_data.inc, the Japanese version's combat and
item numbers for the japanese_balance switch, from the US and Japanese discs.

    python3 tools/build/make_japanese_balance.py US_DISC JP_DISC
    python3 tools/build/make_japanese_balance.py US_DISC JP_DISC --check

Each disc is an extracted disc folder. --check makes it again and fails when
it doesn't match the committed file.

It takes only numbers, never text, and only where the two versions have the
same thing: the monster level for each rank, the power of each weapon
technique, and in each scene the stats, growth and drops of monsters both
versions have, the items they leave, guests' stats, pet equipment and the
forging materials. A monster that's a different one on the Japanese disc,
and every monster's attacks, stay as they are on the US disc.
"""

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / 'src/mods/japanese_balance_data.inc'
DECOMP = REPO / 'game'

EXECUTABLE = {'us': 'SLUS_010.13', 'jp': 'SLPS_021.70'}
OVERLAY_BASE = {'us': 0x8004FC70, 'jp': 0x8004FDD8}
OVERLAY_HEADER_BYTES = 1
SCENE_RESOURCE_BASE = 1548

LEVEL_RANKS = 64
ACTION_TABLE_WEAPON_TYPE = 1
ACTION_BYTES = 8
ACTION_POWER = 2

SCENE_RECORDS = 0x10
SCENE_ACTORS = 0x14

RESOURCE_BATTLE = 1
RESOURCE_GUESTS = 3
RESOURCE_ITEM_TABLE = 4
RESOURCE_PET_WEAPONS = 0xD
RESOURCE_PET_ARMOR = 0xE

ITEM_BYTES = 64
ITEM_NAME_BYTES = 0x14
ITEM_INFO = (0x14, 0x18)
ITEM_NUMBERS = (0x18, 0x38)

TEMPLATE_ID = 0x18
TEMPLATE_NUMBERS = [(0x15, 0x18), (0x19, 0x3F), (0x40, 0x50)]

REWARD_BYTES = 4 + ITEM_BYTES

CHARACTER_BYTES = 592
CHARACTER_NAME_BYTES = 24
CHARACTER_ITEMS = 0x90
CHARACTER_ITEM_COUNT = 7
GUEST_BANKS = 4
GUEST_BYTES = 4 + GUEST_BANKS * CHARACTER_BYTES

ITEM_TABLE_MATERIALS = 4 + 16 * 12 + 16 * 12
MATERIAL_BYTES = 20
MATERIAL_NUMBERS = (4, 12)
MATERIAL_COUNT = 64

MERGE_GAP = 4


def symbol(version, unit, name):
    """A symbol's address from the decomp's symbol file."""
    text = (DECOMP / 'config' / version / 'symbols' / f'{unit}_symbol_addrs.txt').read_text()
    match = re.search(rf'^{re.escape(name)}\s*=\s*(0x[0-9A-Fa-f]+);', text, re.MULTILINE)
    if not match:
        sys.exit(f'make_japanese_balance: {name} isn\'t in the {version} {unit} symbols')
    return int(match.group(1), 16)


def field_overlay(disc):
    """FIELD.BIN, decompressed."""
    sys.path.insert(0, str(DECOMP))
    from tools.compression.decompress import decompress
    return decompress((disc / 'BIN' / 'FIELD.BIN').read_bytes())


def field_bytes(version, overlay, name, size):
    """Bytes of a field overlay global."""
    start = symbol(version, 'field', name) - OVERLAY_BASE[version] + OVERLAY_HEADER_BYTES
    return overlay[start:start + size]


def technique_powers(version, overlay):
    """The power byte of every weapon technique, table 1 of the default action bank."""
    bank = field_bytes(version, overlay, 'g_field_default_action_bank', 16)
    offsets = struct.unpack('<4i', bank)
    count = (offsets[ACTION_TABLE_WEAPON_TYPE + 1] - offsets[ACTION_TABLE_WEAPON_TYPE]) // ACTION_BYTES
    table = field_bytes(version, overlay, 'g_field_default_action_bank', offsets[ACTION_TABLE_WEAPON_TYPE + 1])
    return bytes(table[offsets[ACTION_TABLE_WEAPON_TYPE] + ACTION_BYTES * i + ACTION_POWER] for i in range(count))


def scene_paths(us_disc):
    """Scene number to its file on the US disc, from the disc's resource list."""
    rows = re.findall(r'\{0x([0-9A-F]+), "([^"]+)", (\d+)\}', (REPO / 'src/cd/disc_resource_paths.inc').read_text())
    scenes = {}
    for number, path, _ in rows:
        number = int(number, 16)
        if number >= SCENE_RESOURCE_BASE and path.startswith('ANA/') and path.endswith('.IMG'):
            scenes[number - SCENE_RESOURCE_BASE] = path
    return scenes


def records(scene):
    """A scene file's records section, and its resources by id."""
    start, end = struct.unpack_from('<2I', scene, SCENE_RECORDS)
    section = scene[start:end]
    if len(section) < 4:
        return section, {}
    count = struct.unpack_from('<I', section, 0)[0] >> 2
    if count * 4 > len(section):
        return section, {}
    offsets = [struct.unpack_from('<I', section, 4 * i)[0] for i in range(count)]
    resources = {}
    for offset in offsets:
        if offset + 2 <= len(section):
            resources.setdefault(struct.unpack_from('<H', section, offset)[0], offset)
    return section, resources


def fnv1a(data):
    """32-bit FNV-1a, as the game checks a scene's records with."""
    value = 0x811C9DC5
    for byte in data:
        value = ((value ^ byte) * 0x01000193) & 0xFFFFFFFF
    return value


class Patches:
    def __init__(self, us, jp):
        """The bytes to change in one scene's records section, from its US and Japanese sections."""
        self.us = us
        self.jp = jp
        self.changed = bytearray(us)

    def take(self, us_start, jp_start, length):
        """Take the Japanese bytes for a range, where both sections have it."""
        if us_start + length > len(self.us) or jp_start + length > len(self.jp):
            return
        self.changed[us_start:us_start + length] = self.jp[jp_start:jp_start + length]

    def runs(self):
        """The changed ranges, as (offset, bytes)."""
        differing = [i for i in range(len(self.us)) if self.changed[i] != self.us[i]]
        spans = []
        for i in differing:
            if spans and i - spans[-1][1] <= MERGE_GAP:
                spans[-1][1] = i
            else:
                spans.append([i, i])
        return [(start, bytes(self.changed[start:end + 1])) for start, end in spans]


def battle(patches, us_at, jp_at):
    """Monsters both versions have, and the items they leave."""
    us, jp = patches.us, patches.jp
    us_templates = us_at + struct.unpack_from('<i', us, us_at + 4)[0]
    jp_templates = jp_at + struct.unpack_from('<i', jp, jp_at + 4)[0]
    us_count = struct.unpack_from('<I', us, us_templates)[0]
    jp_count = struct.unpack_from('<I', jp, jp_templates)[0]
    for k in range(min(us_count, jp_count)):
        us_template = us_templates + struct.unpack_from('<I', us, us_templates + 4 + 4 * k)[0]
        jp_template = jp_templates + struct.unpack_from('<I', jp, jp_templates + 4 + 4 * k)[0]
        if us[us_template + TEMPLATE_ID] != jp[jp_template + TEMPLATE_ID]:
            continue
        for start, end in TEMPLATE_NUMBERS:
            patches.take(us_template + start, jp_template + start, end - start)

    us_rewards = us_at + struct.unpack_from('<i', us, us_at + 8)[0]
    jp_rewards = jp_at + struct.unpack_from('<i', jp, jp_at + 8)[0]
    us_count = struct.unpack_from('<H', us, us_rewards + 2)[0]
    jp_count = struct.unpack_from('<H', jp, jp_rewards + 2)[0]
    if us_count != jp_count:
        return
    for k in range(us_count):
        us_entry = us_rewards + 4 + REWARD_BYTES * k
        jp_entry = jp_rewards + 4 + REWARD_BYTES * k
        if us[us_entry:us_entry + 4] != jp[jp_entry:jp_entry + 4]:
            continue
        us_item, jp_item = us_entry + 4, jp_entry + 4
        if us[us_item + ITEM_INFO[0]:us_item + ITEM_INFO[1]] != jp[jp_item + ITEM_INFO[0]:jp_item + ITEM_INFO[1]]:
            continue
        patches.take(us_item + ITEM_NUMBERS[0], jp_item + ITEM_NUMBERS[0], ITEM_NUMBERS[1] - ITEM_NUMBERS[0])


def item(patches, us_item, jp_item):
    """An item's numbers, when it's the same kind of item in both."""
    us, jp = patches.us, patches.jp
    if us[us_item + ITEM_INFO[0]:us_item + ITEM_INFO[1]] == jp[jp_item + ITEM_INFO[0]:jp_item + ITEM_INFO[1]]:
        patches.take(us_item + ITEM_NUMBERS[0], jp_item + ITEM_NUMBERS[0], ITEM_NUMBERS[1] - ITEM_NUMBERS[0])


def guests(patches, us_at, jp_at):
    """Guests' stats and equipment, for guests both versions have."""
    us, jp = patches.us, patches.jp
    count = struct.unpack_from('<H', us, us_at + 2)[0]
    if count != struct.unpack_from('<H', jp, jp_at + 2)[0]:
        return
    for g in range(count):
        us_guest = us_at + 4 + GUEST_BYTES * g
        jp_guest = jp_at + 4 + GUEST_BYTES * g
        if us[us_guest:us_guest + 4] != jp[jp_guest:jp_guest + 4]:
            continue
        for bank in range(GUEST_BANKS):
            us_character = us_guest + 4 + CHARACTER_BYTES * bank
            jp_character = jp_guest + 4 + CHARACTER_BYTES * bank
            patches.take(us_character + CHARACTER_NAME_BYTES, jp_character + CHARACTER_NAME_BYTES, CHARACTER_ITEMS - CHARACTER_NAME_BYTES)
            for i in range(CHARACTER_ITEM_COUNT):
                item(patches, us_character + CHARACTER_ITEMS + ITEM_BYTES * i, jp_character + CHARACTER_ITEMS + ITEM_BYTES * i)


def pet_equipment(patches, us_at, jp_at):
    """Pet weapon or armor templates."""
    end = min(len(patches.us) - us_at, len(patches.jp) - jp_at)
    for i in range((end - 4) // ITEM_BYTES):
        item(patches, us_at + 4 + ITEM_BYTES * i, jp_at + 4 + ITEM_BYTES * i)


def materials(patches, us_at, jp_at):
    """The forging materials' weights and multipliers."""
    for i in range(MATERIAL_COUNT):
        start = ITEM_TABLE_MATERIALS + MATERIAL_BYTES * i + MATERIAL_NUMBERS[0]
        patches.take(us_at + start, jp_at + start, MATERIAL_NUMBERS[1] - MATERIAL_NUMBERS[0])


HANDLERS = {
    RESOURCE_BATTLE: battle,
    RESOURCE_GUESTS: guests,
    RESOURCE_ITEM_TABLE: materials,
    RESOURCE_PET_WEAPONS: pet_equipment,
    RESOURCE_PET_ARMOR: pet_equipment,
}


def scene_patches(us_disc, jp_disc):
    """Each scene's changes: (scene, records hash, [(offset, bytes)])."""
    out = []
    for scene, path in sorted(scene_paths(us_disc).items()):
        us_file, jp_file = us_disc / path, jp_disc / path
        if not jp_file.is_file():
            continue
        try:
            us_section, us_resources = records(us_file.read_bytes())
            jp_section, jp_resources = records(jp_file.read_bytes())
        except struct.error:
            continue
        patches = Patches(us_section, jp_section)
        for resource, handler in HANDLERS.items():
            if resource in us_resources and resource in jp_resources:
                try:
                    handler(patches, us_resources[resource], jp_resources[resource])
                except struct.error:
                    pass
        runs = patches.runs()
        if runs:
            out.append((scene, fnv1a(us_section), runs))
    return out


def c_bytes(data, indent='    '):
    """Bytes as C initializer lines."""
    lines = []
    for i in range(0, len(data), 16):
        lines.append(indent + ', '.join(f'0x{b:02X}' for b in data[i:i + 16]) + ',')
    return '\n'.join(lines)


def make(us_disc, jp_disc):
    """The file's text."""
    us_overlay, jp_overlay = field_overlay(us_disc), field_overlay(jp_disc)
    levels = field_bytes('jp', jp_overlay, 'g_field_monster_level_by_rank', LEVEL_RANKS)
    us_powers, jp_powers = technique_powers('us', us_overlay), technique_powers('jp', jp_overlay)
    if len(us_powers) != len(jp_powers):
        sys.exit('make_japanese_balance: the two versions have a different number of techniques')
    scenes = scene_patches(us_disc, jp_disc)

    patch_rows, data = [], bytearray()
    scene_rows = []
    for scene, records_hash, runs in scenes:
        scene_rows.append(f'    {{{scene}, 0x{records_hash:08X}u, {len(patch_rows)}, {len(runs)}}},')
        for offset, changed in runs:
            patch_rows.append(f'    {{{offset}, {len(changed)}, {len(data)}}},')
            data += changed

    return f"""/* Generated by tools/build/make_japanese_balance.py from the US and Japanese
 * discs; do not edit. The Japanese version's numbers only, no text. */
static const unsigned char k_japanese_level_by_rank[{LEVEL_RANKS}] = {{
{c_bytes(levels)}
}};

static const unsigned char k_japanese_technique_powers[{len(jp_powers)}] = {{
{c_bytes(jp_powers)}
}};

static const LomJapaneseBalanceScene k_japanese_scenes[{len(scene_rows)}] = {{
{chr(10).join(scene_rows)}
}};

static const LomJapaneseBalancePatch k_japanese_patches[{len(patch_rows)}] = {{
{chr(10).join(patch_rows)}
}};

static const unsigned char k_japanese_patch_bytes[{len(data)}] = {{
{c_bytes(data)}
}};
"""


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('us_disc', type=Path)
    parser.add_argument('jp_disc', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for disc, executable in ((args.us_disc, EXECUTABLE['us']), (args.jp_disc, EXECUTABLE['jp'])):
        if not (disc / executable).is_file():
            sys.exit(f'make_japanese_balance: {disc} has no {executable}')
    text = make(args.us_disc, args.jp_disc)
    if args.check:
        if not OUT.is_file() or OUT.read_text() != text:
            sys.exit(f'make_japanese_balance: {OUT.relative_to(REPO)} is out of date; run this without --check')
        print(f'{OUT.relative_to(REPO)} is up to date')
        return
    OUT.write_text(text)
    print(f'wrote {OUT.relative_to(REPO)}')


if __name__ == '__main__':
    main()
