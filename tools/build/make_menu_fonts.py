#!/usr/bin/env python3
import argparse
from pathlib import Path
import shutil

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

DESCRIPTION = """Make the in-game menu's fonts in src/ui/fonts/ from Google Fonts' files.

    python3 tools/build/make_menu_fonts.py FOLDER
    python3 tools/build/make_menu_fonts.py --cjk FOLDER

With --cjk, FOLDER has NotoSansCJK-Regular.ttc and NotoSansCJK-Bold.ttc from
github.com/notofonts/noto-cjk (Sans/OTC), and this makes the menu's
Japanese, Korean and Chinese fallbacks: each language's own face (JP, KR, SC,
TC), cut down to the characters its translation in src/ui/translations/
uses, which brings 20 MB down to about 50 KB each. Run it again after a
translation changes; the menu_text smoke test says when it's needed.

FOLDER has Alegreya[wght].ttf, Alegreya-Italic[wght].ttf,
AtkinsonHyperlegible-Regular.ttf and AtkinsonHyperlegible-Bold.ttf from
github.com/google/fonts (ofl/alegreya and ofl/atkinsonhyperlegible). It needs
fontTools (pip install fonttools).

Alegreya only comes as a variable font, and ImGui's font loader reads a
variable font's default weight and nothing else. So this pins the two weights
the menu uses, Bold for the title and Medium Italic for headings, as static
fonts, and keeps only Latin text, which brings them from 420 KB to about
60 KB each. Atkinson Hyperlegible already comes as static fonts and is copied
as it is.

Both are under the SIL Open Font License, with no Reserved Font Name, so
instancing and subsetting them is allowed. Their licenses are in
packaging/licenses/.
"""

REPO = Path(__file__).resolve().parents[2]
OUT_DIR = REPO / 'src/ui/fonts'

ALEGREYA = (
    ('Alegreya[wght].ttf', 700, 'Alegreya-Bold.ttf'),
    ('Alegreya-Italic[wght].ttf', 500, 'Alegreya-MediumItalic.ttf'),
)

ATKINSON = ('AtkinsonHyperlegible-Regular.ttf', 'AtkinsonHyperlegible-Bold.ttf')

LATIN = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) + list(range(0x2010, 0x2028))


def make_alegreya(source, weight, output):
    """Make one weight of Alegreya, cut down to the Latin letters the menu uses.

    @param source The variable font.
    @param weight The weight.
    @param output Where to write it.
    """
    font = TTFont(source)
    static = instancer.instantiateVariableFont(font, {'wght': weight})
    options = subset.Options()
    options.layout_features = ['kern', 'liga', 'calt']
    options.name_IDs = ['*']
    options.notdef_outline = True
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=LATIN)
    subsetter.subset(static)
    static.save(output)


CJK = (('ja', 0, 'JP'), ('ko', 1, 'KR'), ('zh_CN', 2, 'SC'), ('zh_TW', 3, 'TC'))


def make_cjk(folder):
    """Make the CJK fallback fonts, cut down to the characters each translation uses.

    @param folder The folder with Noto Sans CJK's collections.
    """
    import json
    from fontTools.ttLib import TTCollection
    for code, face, region in CJK:
        texts = json.loads((REPO / 'src/ui/translations' / f'{code}.json').read_text(encoding='utf-8'))
        characters = sorted({ord(c) for text in texts.values() for c in text})
        for weight in ('Regular', 'Bold'):
            font = TTCollection(folder / f'NotoSansCJK-{weight}.ttc').fonts[face]
            options = subset.Options()
            options.name_IDs = ['*']
            options.notdef_outline = True
            options.hinting = False
            subsetter = subset.Subsetter(options)
            subsetter.populate(unicodes=characters)
            subsetter.subset(font)
            output = OUT_DIR / f'NotoSansCJK{region}-{weight}.otf'
            font.save(output)
            print('Made', output, len(characters), 'characters')


def main():
    """Make the menu's fonts."""
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('folder', type=Path, help="the folder with Google Fonts' files")
    parser.add_argument('--cjk', action='store_true', help="make the CJK fallbacks from Noto Sans CJK's collections instead")
    args = parser.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    if args.cjk:
        make_cjk(args.folder)
        return
    for source, weight, output in ALEGREYA:
        make_alegreya(args.folder / source, weight, OUT_DIR / output)
        print('Made', OUT_DIR / output)
    for name in ATKINSON:
        shutil.copyfile(args.folder / name, OUT_DIR / name)
        print('Copied', OUT_DIR / name)


if __name__ == '__main__':
    main()
