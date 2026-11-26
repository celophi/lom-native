from __future__ import annotations

import json
import re
from pathlib import Path

import lom_saves
import lom_scenes

from .settings import ANY_SCENE, COMPANION_VARIANT, GUEST_VARIANT, HERE, LAND_DISTANCE, PARKED_POSITION, SAVED_GAME_KINDS, WORLD_MAP_SCENE


SCHEMA = HERE / "route.schema.json"
DEFAULT_FRAME_LIMIT = 20000
TITLE_LINES = {"new_game": "title 0", "new_game_back": "title 0 back", "continue_up": "title 1 up", "continue_after_idle": "title 1 idle"}
OTHER_GAME_TITLE = "ＴＥＳＴ　ＳＡＶＥ"
CARD_BLOCK_BYTES = 8192


def other_game_save() -> bytes:
    """The card file for another game's save."""
    header = b"SC" + bytes([0x11, 1]) + OTHER_GAME_TITLE.encode("shift_jis")
    return header.ljust(CARD_BLOCK_BYTES, b"\0")


JOIN_GUEST_OPCODE = 0x47
JOIN_COMPANION_OPCODE = 0x48
COMPANION_TEMPLATES_RESOURCE = 7
ANSWER_LIMIT = 4
NO_RECORD = -1
STORY_SEARCH_LIMIT = 20000
BUTTONS = {
    "select": 0x0001, "start": 0x0008, "up": 0x0010, "right": 0x0020, "down": 0x0040, "left": 0x0080,
    "l2": 0x0100, "r2": 0x0200, "l1": 0x0400, "r1": 0x0800, "triangle": 0x1000, "circle": 0x2000, "cross": 0x4000, "square": 0x8000,
}
CALCULATIONS = {
    "add": lambda a, b: a + b, "subtract": lambda a, b: a - b, "multiply": lambda a, b: a * b,
    "bit_and": lambda a, b: a & b, "bit_or": lambda a, b: a | b, "bit_xor": lambda a, b: a ^ b,
    "unsigned_maximum": max, "unsigned_minimum": min,
}
STORY_CALL_DEPTH = 16
DIRECT_AUDIO = 0xFEFEFE


class RouteError(Exception):
    pass


def validate(value, schema: dict, root: dict, where: str = "route") -> None:
    """Check a value against the subset of JSON Schema that route.schema.json uses."""
    if "$ref" in schema:
        validate(value, root["$defs"][schema["$ref"].split("/")[-1]], root, where)
        return
    if "oneOf" in schema:
        matches = 0
        for option in schema["oneOf"]:
            try:
                validate(value, option, root, where)
                matches += 1
            except RouteError:
                pass
        if matches != 1:
            keys = ", ".join(sorted(value)) if isinstance(value, dict) else repr(value)
            raise RouteError(f"{where}: not a valid step or value ({keys})")
        return
    kind = schema.get("type")
    checks = {"object": dict, "array": list, "string": str}
    if kind == "integer" and (not isinstance(value, int) or isinstance(value, bool)):
        raise RouteError(f"{where}: should be a whole number")
    if kind in checks and not isinstance(value, checks[kind]):
        raise RouteError(f"{where}: should be a{'n' if kind[0] in 'aeiou' else ''} {kind}")
    if "const" in schema and value != schema["const"]:
        raise RouteError(f"{where}: should be {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        raise RouteError(f"{where}: {value!r} isn't one of {', '.join(map(str, schema['enum']))}")
    if "pattern" in schema and not re.search(schema["pattern"], value):
        raise RouteError(f"{where}: {value!r} doesn't match {schema['pattern']}")
    if "minimum" in schema and value < schema["minimum"]:
        raise RouteError(f"{where}: should be at least {schema['minimum']}")
    if kind == "array":
        if len(value) < schema.get("minItems", 0):
            raise RouteError(f"{where}: needs at least {schema['minItems']} item(s)")
        for index, item in enumerate(value):
            validate(item, schema.get("items", {}), root, f"{where}[{index}]")
    if kind == "object":
        for name in schema.get("required", []):
            if name not in value:
                raise RouteError(f"{where}: missing {name!r}")
        properties = schema.get("properties", {})
        for name, item in value.items():
            if name not in properties:
                if schema.get("additionalProperties") is False:
                    raise RouteError(f"{where}: unknown field {name!r}")
                continue
            validate(item, properties[name], root, f"{where}.{name}")


def load_route(path: Path) -> dict:
    """Read and check a route file."""
    try:
        route = json.loads(path.read_text())
    except json.JSONDecodeError as error:
        raise RouteError(f"{path}: not valid JSON ({error})") from error
    schema = json.loads(SCHEMA.read_text())
    validate(route, schema, schema)
    return route


def via_suffix(step: dict) -> str:
    """The plan's "via x z ..." ending for a step that walks through points first."""
    points = step.get("via", [])
    return (" via " + " ".join(f"{x} {z}" for x, z in points)) if points else ""


def with_music(audio: int, step: dict) -> int:
    """A change_scene audio operand with an enter step's music id in place of its own (byte 1), if it gives one."""
    return audio if "music" not in step else (audio & ~0xFF00) | (step["music"] << 8)


class Planner:

    def __init__(self, disc: Path, scene: int, map_object: int):
        """Start planning in a scene (and the map object the save's spawn is on)."""
        self.disc = disc
        self.scene = scene
        self.map_object = map_object
        self.lines: list[str] = []

    def enter(self, step: dict) -> None:
        """Plan an enter step: the change_scene in the game data that comes in that way, or a direct entry."""
        if step.get("direct"):
            self.lines.append(f"enter {step['scene']} 0 {with_music(DIRECT_AUDIO, step)} {step.get('spawn', 0)}")
            self.scene, self.map_object = step["scene"], 0
            return
        candidates = [c for c in lom_scenes.change_scenes_into(self.disc, step["scene"])
                      if c["from"] == step.get("from", c["from"]) and c["spawn_id"] == step.get("spawn", c["spawn_id"])]
        if not candidates:
            raise RouteError(f"no change_scene in the game data enters scene {step['scene']} that way")
        change = candidates[0]
        self.lines.append(f"enter {change['scene_id']} {change['object_id']} {with_music(change['audio'], step)} {change['spawn_id']}")
        self.scene, self.map_object = step["scene"], change["object_id"]

    def walk_to(self, step: dict) -> None:
        """Plan a walk_to step: to a point, or onto an exit's marker, expecting the scene the exit leads to."""
        if "x" in step:
            self.lines.append(f"walk {step['x']} {step['z']}{via_suffix(step)}")
            return
        if self.scene is None:
            raise RouteError("the route doesn't know which scene it's in (an earlier exit could lead to more than one); give that exit a \"to\"")
        here = lom_scenes.scene(self.disc, self.scene)
        markers = [m for m in here.markers(self.map_object) if m.label == step["exit"]]
        if not markers or not markers[0].targets:
            raise RouteError(f"scene {self.scene} has no exit {step['exit']}")
        targets = markers[0].targets
        target = step.get("to", targets[0] if len(targets) == 1 else ANY_SCENE)
        if target != ANY_SCENE and target not in targets:
            raise RouteError(f"exit {step['exit']} of scene {self.scene} doesn't lead to {target}")
        clearance = f" {step.get('clearance', 0)} 1" if step.get("event_may_move") else f" {step['clearance']}" if step.get("clearance") else ""
        self.lines.append(f"exit {step['exit']} {target}{clearance}{via_suffix(step)}")
        objects = [c["object_id"] for c in here.change_scenes(step["exit"])
                   if c.get("scene_id") is not None and c["scene_id"] & lom_scenes.SCENE_MASK == target]
        self.scene, self.map_object = (target, objects[0] if objects else 0) if target != ANY_SCENE else (None, 0)

    def talk_to(self, step: dict) -> None:
        """Plan a talk_to step: to a point, or to a layout record where the scene places it."""
        x, z = (step["x"], step["z"]) if "x" in step else lom_scenes.scene(self.disc, self.scene).layout_position(step["record"])
        parked = "record" in step and (x >= PARKED_POSITION or z >= PARKED_POSITION)
        flags = [int(bool(step.get("event_may_leave"))), int(bool(step.get("long"))), step.get("clearance", 0), step.get("record", NO_RECORD),
                 int(bool(step.get("avoid_regions"))), int(parked), int(bool(step.get("until_screen"))), int(bool(step.get("follow")))]
        defaults = [0, 0, 0, NO_RECORD, 0, 0, 0, 0]
        while flags and flags[-1] == defaults[len(flags) - 1]:
            flags.pop()
        self.lines.append(f"talk {x} {z}{''.join(f' {flag}' for flag in flags)}{via_suffix(step)}")

    def press(self, press: dict) -> None:
        """Plan a press step: the buttons, held for some frames."""
        bits = 0
        for button in press["buttons"]:
            bits |= BUTTONS[button]
        self.lines.append(f"press {bits:#06x} {press.get('frames', 1)}")

    def wait_until(self, until: dict) -> None:
        """Plan a wait for a scene, which the plan then follows into, or for a battle."""
        if "scene" in until:
            self.lines.append(f"wait_scene {until['scene']} {until['max_frames']}")
            self.scene = until["scene"]
        else:
            self.lines.append(f"wait_battle {until['max_frames']}")

    def event(self, event: dict) -> None:
        """Plan starting a layout record's event script, found in the scene's layout."""
        lom_scenes._decomp_tools()
        from tools.data.scenes.layout import read_layout_records

        here = lom_scenes.scene(self.disc, self.scene)
        record = read_layout_records(here.data, here.header)[event["record"]]
        self.lines.append(f"event {event['record']} {record.scripts[event['slot']]}")

    def fight(self, step: dict) -> None:
        """Plan a fight, with the options it was given and none of the defaults after them."""
        extra = [int(bool(step.get("confirm_text"))), int(bool(step.get("rotate_targets"))), step.get("fight_for", 0)]
        while extra and not extra[-1]:
            extra.pop()
        self.lines.append(" ".join(["fight", str(step["fight"]), *map(str, extra)]))

    def place(self, place: dict) -> None:
        """Plan putting an artifact on the world map, which the steps before have to have reached."""
        if self.scene != WORLD_MAP_SCENE:
            raise RouteError("a place step has to follow an exit to the world map")
        self.lines.append(f"place {place['artifact']}")

    def land(self, land: dict) -> None:
        """Plan entering a land from the world map; the plan then follows into its scene."""
        if self.scene != WORLD_MAP_SCENE:
            raise RouteError("a land step has to follow an exit to the world map")
        self.lines.append(f"land {land['index']} {land.get('scene', ANY_SCENE)}")
        self.scene, self.map_object = land.get("scene"), 0

    def step(self, step: dict) -> None:
        """Add one route step to the plan."""
        if "enter" in step:
            self.enter(step["enter"])
        elif "walk_to" in step:
            self.walk_to(step["walk_to"])
        elif "talk_to" in step:
            self.talk_to(step["talk_to"])
        elif "press" in step:
            self.press(step["press"])
        elif "wait" in step:
            self.lines.append(f"wait {step['wait']}")
        elif "wait_until" in step:
            self.wait_until(step["wait_until"])
        elif "checkpoint" in step:
            self.lines.append(f"checkpoint {step['checkpoint']}")
        elif "guest" in step:
            self.lines.append(f"guest {step['guest']}")
        elif "companion" in step:
            self.lines.append(f"companion {step['companion']}")
        elif "answer" in step:
            self.lines.append(f"answer {step['answer']}")
        elif "widescreen" in step:
            self.lines.append(f"widescreen {int(bool(step['widescreen']))}")
        elif "always_run" in step:
            self.lines.append(f"always_run {int(bool(step['always_run']))}")
        elif "switch" in step:
            self.lines.append(f"switch {step['switch']['name']} {int(bool(step['switch']['on']))}")
        elif "event" in step:
            self.event(step["event"])
        elif "audio" in step:
            self.lines.append(" ".join(["audio", step["audio"]["call"], *(str(a) for a in step["audio"].get("args", []))]))
        elif "fight" in step:
            self.fight(step)
        elif "experience" in step:
            self.lines.append(f"experience {step['experience']}" + (f" {step['character']}" if step.get("character") else ""))
        elif "hit_points" in step:
            self.lines.append(f"hit_points {step['hit_points']}")
        elif "watch" in step:
            self.lines.append(f"watch {step['watch']}")
        elif "place" in step:
            self.place(step["place"])
        elif "land" in step:
            self.land(step["land"])


def story_alternatives(disc: Path, number: int, label: int, target: int, default=lambda reference: 0) -> list[dict[int, int]]:
    """The saved-game variable values an exit's script can reach a change_scene to its target with.

    default gives the value a variable has in the save the route starts from.
    A variable keeps that value when it meets every test on the path, so a
    requirement is only what the save doesn't already have. Each distinct set
    of changes is returned, fewest first: an exit can fail for a reason outside
    its own script (a gate placed by another script), and a path that changes
    more can be the one that gets past it.

    A small symbolic run of the script from the marker's entry: calls return
    where they were made, values the script sets from constants (set_variable,
    calculate) are known, and a range test decides its branch when its value
    is known. A test on a saved-game variable whose value isn't known becomes
    a requirement (in range on one branch, out of range on the other); a test
    on an unknown runtime value allows both branches. A path ends at anything
    that can't be followed statically. Routes built from them are validated by
    running them.
    """
    here = lom_scenes.scene(disc, number)
    resolved = here.links.resolve(label)
    if "offset" not in resolved:
        return []

    def reaches(instruction: dict) -> bool:
        """Whether an instruction is the change_scene the search is looking for."""
        scene_id = instruction["operands"]["scene_id"].get("value") if instruction["command"] == "change_scene" else None
        return scene_id is not None and (target == ANY_SCENE or scene_id & lom_scenes.SCENE_MASK == target)

    return story_paths(disc, number, resolved["offset"], reaches, default)


def operand_value(operand: dict, known: dict[int, int]) -> int | None:
    """An operand's value: a constant, or a variable whose value is known on this path. None when it isn't known."""
    if operand.get("value") is not None and "reference" not in operand:
        return operand["value"]
    return known.get(operand.get("reference"))


def solve_value(constraints: tuple, reference: int, default) -> int | None:
    """The save's value if it's inside every (low, high, True) range and outside every (low, high, False) one, else the smallest that is."""
    candidates = [default(reference)] + sorted({0} | {v for low, high, _ in constraints for v in (low, high, low - 1, high + 1) if v >= 0})
    return next((v for v in candidates if all((low <= v <= high) == inside for low, high, inside in constraints)), None)


def chosen_values(needs: dict, default) -> dict[int, int]:
    """The value each required variable gets on a path."""
    return {reference: solve_value(constraints, reference, default) for reference, constraints in needs.items()}


def track_assignment(instruction: dict, known: dict[int, int], needs: dict, default) -> dict[int, int]:
    """What a path knows after an instruction that sets a variable: from a constant or a known value, or, from
    a calculation, the result when both sides are known. Anything else makes the variable unknown."""
    command, operands = instruction["command"], instruction["operands"]
    destination = operands.get("destination", {})
    if "reference" not in destination:
        return known
    known = dict(known)
    if command in ("set_variable", "write_variable"):
        value = operand_value(operands.get("value", {}), known)
    else:
        both = {**chosen_values(needs, default), **known}
        left, right = operand_value(operands["left"], both), operand_value(operands["right"], both)
        operation = CALCULATIONS.get(instruction.get("calculation"))
        value = None if left is None or right is None or operation is None else operation(left, right)
    if value is None:
        known.pop(destination["reference"], None)
    else:
        known[destination["reference"]] = value
    return known


def branch_edges(command: str, edges: dict, test: dict | None, known: dict[int, int], needs: dict, saved, default) -> list:
    """The ways on from a conditional branch, after the range test before it. A test whose value is known picks
    its way; one on a saved-game variable that isn't known adds a requirement to each way that can meet it; one
    on anything else allows both. Returns (offset, needs) for each."""
    follow = []
    taken = "branch_false" if command == "branch_if_false" else "branch_true"
    for kind, wants_true in ((taken, command == "branch_if_true"), ("fallthrough", command == "branch_if_false")):
        if edges.get(kind) is None:
            continue
        edge_needs = needs
        if test is not None:
            reference = test["operands"]["variable"].get("reference")
            low, high = operand_value(test["operands"]["minimum"], {}), operand_value(test["operands"]["maximum"], {})
            current = known.get(reference)
            if low is None or high is None or reference is None:
                pass
            elif current is not None:
                if (low <= current <= high) != wants_true:
                    continue
            elif saved(reference):
                constraints = needs.get(reference, ()) + ((low, high, wants_true),)
                if solve_value(constraints, reference, default) is None:
                    continue
                edge_needs = {**needs, reference: constraints}
        follow.append((edges[kind], edge_needs))
    return follow


def switch_edges(instruction: dict, known: dict[int, int], needs: dict, default) -> list:
    """The ways on from a switch: its matching case (or its default) when the value is known, otherwise every
    case. Returns (offset, needs) for each."""
    key = operand_value(instruction["operands"]["value"], {**chosen_values(needs, default), **known})
    cases = instruction.get("cases", [])
    matching = [case for case in cases if key is not None and case["key"] == key] or [case for case in cases if case.get("default")]
    return [(case["target_offset"], needs) for case in (matching if key is not None else cases) if case.get("target_offset") is not None]


def story_paths(disc: Path, number: int, start: int, goal, default=lambda reference: 0) -> list[dict[int, int]]:
    """The saved-game variable values a script starting at an offset can reach a goal instruction with
    (story_alternatives' search, for any goal). A path also ends at a change_scene that isn't the goal."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.event_scripts import describe_variable

    here = lom_scenes.scene(disc, number)
    by_offset = {instruction["offset"]: instruction for instruction in here.scripts["instructions"]}

    def saved(reference: int) -> bool:
        """Whether a variable lives in the saved game, so a built save can set it."""
        return describe_variable(reference)["variable_kind"] < SAVED_GAME_KINDS or reference == LAND_DISTANCE

    found: dict[frozenset, dict[int, int]] = {}
    stack = [(start, (), None, {}, {}, frozenset())]
    for _ in range(STORY_SEARCH_LIMIT):
        if not stack:
            break
        offset, returns, test, known, needs, visited = stack.pop()
        instruction = by_offset.get(offset)
        if instruction is None or (offset, returns) in visited or len(returns) > STORY_CALL_DEPTH:
            continue
        visited = visited | {(offset, returns)}
        command = instruction["command"]
        edges = {edge["kind"]: edge.get("target_offset") for edge in instruction["successors"]}
        if goal(instruction):
            changed = {reference: value for reference, value in chosen_values(needs, default).items() if value != default(reference)}
            found.setdefault(frozenset(changed.items()), changed)
            continue
        if command == "change_scene":
            continue
        if command == "return":
            if returns:
                stack.append((returns[-1], returns[:-1], None, known, needs, visited))
            continue
        if command == "call":
            if edges.get("call") is not None and edges.get("fallthrough") is not None:
                stack.append((edges["call"], returns + (edges["fallthrough"],), None, known, needs, visited))
            continue
        if command == "test_unsigned_range":
            test = instruction
        elif command in ("set_variable", "write_variable", "calculate"):
            known = track_assignment(instruction, known, needs, default)
        if command in ("branch_if_false", "branch_if_true"):
            follow = branch_edges(command, edges, test, known, needs, saved, default)
        elif command == "switch":
            follow = switch_edges(instruction, known, needs, default)
        else:
            follow = [(edges[kind], needs) for kind in ("fallthrough", "jump") if edges.get(kind) is not None]
        for next_offset, edge_needs in follow:
            stack.append((next_offset, returns, None if command.startswith("branch_if") else test, known, edge_needs, visited))
    return sorted(found.values(), key=lambda changed: (len(changed), sorted(changed.items())))


def scene_state_candidates(disc: Path, number: int, default) -> list[dict[int, int]]:
    """Single saved-game variable changes worth trying for an exit the walk can't reach.

    Every saved-game variable the scene's scripts test (other than which guest or
    companion is in the party, which need someone to join), with each value its
    tests tell apart that differs from the save's, most-tested variable first.
    """
    lom_scenes._decomp_tools()
    from tools.data.scenes.event_scripts import describe_variable

    tests: dict[int, list[tuple[int, int]]] = {}
    for instruction in lom_scenes.scene(disc, number).scripts["instructions"]:
        if instruction["command"] != "test_unsigned_range":
            continue
        reference = instruction["operands"]["variable"].get("reference")
        low, high = instruction["operands"]["minimum"].get("value"), instruction["operands"]["maximum"].get("value")
        if reference is None or low is None or high is None or reference in (GUEST_VARIANT, COMPANION_VARIANT):
            continue
        if describe_variable(reference)["variable_kind"] < SAVED_GAME_KINDS:
            tests.setdefault(reference, []).append((low, high))
    candidates = []
    for reference, ranges in sorted(tests.items(), key=lambda item: -len(item[1])):
        values = sorted({v for low, high in ranges for v in (low, high + 1) if v != default(reference)})
        candidates += [{reference: value} for value in values]
    return candidates


def guest_join_scenes(disc: Path, guest: int, target: int, area_numbers: set[int]) -> list[int]:
    """Scenes whose scripts have a guest join (where the game loads that guest's templates), best first:
    the target scene, then its area, then the rest."""
    scenes = []
    for number in range(lom_scenes.SCENE_COUNT):
        for instruction in lom_scenes.scene(disc, number).scripts["instructions"]:
            if instruction["opcode"] == JOIN_GUEST_OPCODE and any("guest" in name and operand.get("value") == guest
                                                                  for name, operand in instruction["operands"].items()):
                scenes.append(number)
                break
    return sorted(scenes, key=lambda number: (number != target, number not in area_numbers, number))


def companion_join_scenes(disc: Path, target: int, area_numbers: set[int]) -> list[tuple[int, int]]:
    """Scenes where a story pet joins (a new companion, from the scene's own templates), as
    (scene, template), best first: the target scene, then its area, then the rest."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.scene_resources import read_resource_directory

    scenes = []
    for number in range(lom_scenes.SCENE_COUNT):
        here = lom_scenes.scene(disc, number)
        if not any(instruction["opcode"] == JOIN_COMPANION_OPCODE and instruction["operands"]["source"].get("value") == 0
                   for instruction in here.scripts["instructions"]):
            continue
        _, resources = read_resource_directory(here.data[here.header.records:here.header.actors])
        if any(resource.data and int.from_bytes(resource.data[:2], "little") == COMPANION_TEMPLATES_RESOURCE for resource in resources):
            scenes.append((number, 0))
    return sorted(scenes, key=lambda join: (join[0] != target, join[0] not in area_numbers, join[0]))


def choice_answers(disc: Path, number: int, label: int) -> list[int]:
    """The answers other than the first to try for the text choices an exit's script shows (a
    "Wait / Let's go!", say), or none when it shows no choice."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.strings import read_strings

    here = lom_scenes.scene(disc, number)
    options = {entry["index"]: sum(token.get("command") == "choice" for token in entry.get("tokens", []))
               for entry in read_strings(here.data, here.header)["strings"]}
    most = max((options.get(instruction["operands"]["string_index"].get("value"), 0)
                for instruction in here.links.resolve(label)["instructions"] if instruction["command"] == "show_text"), default=0)
    return list(range(1, min(most, ANSWER_LIMIT + 1)))


def save_description(route_path: Path, route: dict) -> dict:
    """The route's save description."""
    path = route_path.parent / route["start"]["save"]
    description = lom_saves.load_description(path)
    if "scene" not in description.get("scene", {}):
        raise RouteError(f"{path}: a route's save has to say which scene it's in")
    return description


def make_plan(route_path: Path, route: dict, disc: Path) -> str:
    """The runner's plan for a route."""
    place = save_description(route_path, route)["scene"]
    planner = Planner(disc, place["scene"], place.get("object", 0))
    if route["steps"] and ("place" in route["steps"][0] or "land" in route["steps"][0]):
        planner.scene = WORLD_MAP_SCENE
    for index, step in enumerate(route["steps"]):
        try:
            planner.step(step)
        except RouteError as error:
            raise RouteError(f"{route_path} step {index}: {error}") from error
    header = [f"# {route['id']}: {route['description']}", f"limit {route.get('frame_limit', DEFAULT_FRAME_LIMIT)}"]
    if "title" in route:
        header.append(TITLE_LINES[route["title"]])
    if route.get("card_other_game"):
        header.append("card_browse")
    return "\n".join(header + planner.lines) + "\n"
