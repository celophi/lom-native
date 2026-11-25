from __future__ import annotations

from pathlib import Path


HERE = Path(__file__).resolve().parent.parent
BASELINE_FORMAT = 3
COMPACT_FORMAT = 2
BASELINE_SUFFIX = ".baseline.json"
ROUTES_ROOT = HERE.parent.parent / "tests" / "routes"
MODS_ROOT = HERE.parent.parent / "tests" / "mods"
BUNDLED_MODS_ROOT = HERE.parent.parent / "launcher" / "data" / "mods"
NO_MODS_FOLDER = "no-mods"
MODS_LIST = "mods.ini"
MODS_CARDS_PREFIX = "mods-"
MODS_CARDS_HASH_DIGITS = 12
SET_ENTRY_SEPARATOR = "#"
CUTSCENE_FRAME_LIMIT = 60000
EVENT_WATCH_FRAMES = 6000
NEVER_ENDED = "the conversation never ended"
IN_A_BATTLE = "(in a battle)"
CLEARANCES = (8, 16)
GUEST_VARIANT = 0x2F08
NO_GUEST = 0xFF
COMPANION_VARIANT = 0x2F00
SAVED_GAME_KINDS = 3
LAND_DISTANCE = 0x5320
WMAP_LAYOUT_RESOURCE = 0x12EA
WMAP_LAYOUT_FILES = 64
WMAP_LAYOUT_BYTES = 1208
WMAP_SCENES_OFFSET = 440
WMAP_LANDS = 64
WMAP_SCENE_VARIANTS = 4
WMAP_NO_SCENE = (0, 0xFFFF)
PARKED_POSITION = 1000
LAND_PLACED_FLAGS = 0x0B
MAX_EXPERIENCE = 9999999
ANY_SCENE = -1
WORLD_MAP_SCENE = 0x7FFF
CHANNELS = ("scene", "hero", "save", "state", "picture", "sound")
SAVE_DUMP = "save-{}.bin"
WALKED_INTO_SCENE_CHANGE = "started while walking"
EVENT_SLOTS = range(14)
TALK_ENTRY_TRIES = 3
STORY_ROUTE_TRIES = 4
LAND_WATCH_FRAMES = 300
PLACEMENT_LEFT_THE_MAP = "handed over to the field during the place step"
LAND_NOT_JOINED = "no chain of lands joins"
ARTIFACT_PLACED_AS = {22: 16}
MENU_PRESS_FRAMES = 4
MENU_WAIT_FRAMES = 40
BOUNDARY_BASELINE = ROUTES_ROOT / "boundaries.json"
