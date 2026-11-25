from __future__ import annotations

import argparse
from pathlib import Path
from typing import NamedTuple

__all__ = ["compress", "compress_file"]


OP_F0 = 0xF0
OP_F1 = 0xF1
OP_F2 = 0xF2
OP_F3 = 0xF3
OP_F4 = 0xF4
OP_F5 = 0xF5
OP_F6 = 0xF6
OP_F7 = 0xF7
OP_F8 = 0xF8
OP_F9 = 0xF9
OP_FA = 0xFA
OP_FB = 0xFB
OP_FC = 0xFC
OP_FD = 0xFD
OP_FE = 0xFE
OP_END = 0xFF

MAX_LITERAL_RUN = 240
BYTE_MASK = 0xFF
WORD_MASK = 0xFFFF
WORD_MODULUS = 0x10000
NIBBLE_MASK = 0x0F
NIBBLE_STEP = 0x10
SIGNED_BYTE_MIN = -128
SIGNED_BYTE_MAX = 127
SIGNED_WORD_MAX = 32767

F0_MIN_RUN = 3
F0_MAX_RUN = 18
F1_MIN_RUN = 4
F1_MAX_RUN = 259
F2_MIN_PAIRS = 2
F2_MAX_PAIRS = 257
F3_MIN_PAIRS = 2
F3_MAX_PAIRS = 257
F4_MIN_TRIPLES = 2
F4_MAX_TRIPLES = 257
F5_MIN_PAIRS = 4
F5_MAX_PAIRS = 259
F6_MIN_TRIPLES = 3
F6_MAX_TRIPLES = 258
F6_ZERO_FIXED_PROBE_TRIPLES = 3
F7_MIN_QUADS = 2
F7_MAX_QUADS = 257
F8_MIN_RUN = 4
F8_MAX_RUN = 259
F9_MIN_RUN = 4
F9_MAX_RUN = 259
FA_MIN_RUN = 5
FA_MAX_RUN = 260
FB_MIN_PAIRS = 3
FB_MAX_PAIRS = 258

F0_ENCODED_SIZE = 2
F1_ENCODED_SIZE = 3
F2_ENCODED_SIZE = 3
F3_ENCODED_SIZE = 4
F4_ENCODED_SIZE = 5
F5_BASE_ENCODED_SIZE = 3
F6_BASE_ENCODED_SIZE = 4
F7_BASE_ENCODED_SIZE = 5
F8_ENCODED_SIZE = 3
F9_ENCODED_SIZE = 3
FA_ENCODED_SIZE = 4
FB_ENCODED_SIZE = 5

FE_MIN_MATCH = 3
FE_MAX_MATCH = 18
FE_DISTANCE_STEP = 8
FE_MAX_DISTANCE = 128
FC_MIN_MATCH = 4
FC_MAX_MATCH = 19
FC_WINDOW = 4096
FD_MIN_MATCH = 20
FD_MAX_MATCH = 275
FD_WINDOW = 256


EFFICIENCY_FRACTION_BITS = 12
F3_LOOKAHEAD_MIN_PAIRS = 3

FULL_PATTERN_PROBE_BYTES = F7_MAX_QUADS * 4

F4_CONSTANT_SCAN_LIMIT = F4_MAX_TRIPLES * 3 + 2

F5_NEAR_END_TAIL_PAIRS = 5
F5_SHORT_TAIL_PAIRS = 4
F5_SHORT_LZ_PREEMPT_MAX_PAIRS = 16
F5_ZERO_Q1_LONG_MIN_PAIRS = 34
F5_Q2_DEG2_MIN_PAIRS = 6
F5_Q3_DEG2_MIN_PAIRS = 7
F5_ZERO_Q3_LONG_MIN_PAIRS = 8
F5_SECONDARY_PHASE_MODULUS = 4
F5_SECONDARY_PHASE = 3
F5_SECONDARY_MARKER = 0x10
F5_SECONDARY_NEXT_MIN = 0xD0
F5_SECONDARY_SUFFIX_LIMIT = 24

LONG_CONSTANT_F4_MIN_ADVANCE = 513


class PatternCandidate(NamedTuple):
    encoded: bytes
    advance: int
    savings: int
    count: int

    @property
    def opcode(self) -> int:
        return self.encoded[0]


class BackrefCandidate(NamedTuple):
    encoded: bytes
    advance: int
    savings: int
    distance: int
    same_parity: bool

    @property
    def opcode(self) -> int:
        return self.encoded[0]


class F5Scan(NamedTuple):
    fixed_byte: int
    structural_pairs: int
    strict_prefix_pairs: int
    scanned_pairs: int
    scanned_strict_pairs: int
    structural_degenerate_pairs: int
    scanned_degenerate_pairs: int


class F4Scan(NamedTuple):
    first: int
    second: int
    third: int
    triple_count: int


class F6Scan(NamedTuple):
    first_fixed: int
    second_fixed: int
    triple_count: int
    varying: bytes


class F7Scan(NamedTuple):
    first_fixed: int
    second_fixed: int
    third_fixed: int
    quad_count: int
    varying: bytes


class FBScan(NamedTuple):
    first_value: int
    delta: int
    pair_count: int


def compress(src: bytes) -> bytes:
    """Compress ``src`` using the recovered byte-exact BIN encoder policy.

    The returned stream includes the terminating ``0xFF`` opcode.  ``src`` is
    coerced to immutable ``bytes`` so callers may safely pass ``bytearray`` or
    other bytes-like objects without the compressor observing later mutation.
    """
    src = bytes(src)
    n = len(src)
    output = bytearray()
    i = 0
    literal_buffer = bytearray()

    def flush_literals() -> None:
        j = 0
        while j < len(literal_buffer):
            chunk_len = min(MAX_LITERAL_RUN, len(literal_buffer) - j)
            output.append(chunk_len - 1)
            output.extend(literal_buffer[j:j + chunk_len])
            j += chunk_len
        literal_buffer.clear()

    while i < n:
        result = _select_encoding(src, i, n, len(literal_buffer))
        if result is not None:
            flush_literals()
            encoded, advance = result
            output.extend(encoded)
            i += advance
        else:
            literal_buffer.append(src[i])
            i += 1
            if len(literal_buffer) == MAX_LITERAL_RUN:
                flush_literals()

    flush_literals()
    output.append(OP_END)
    return bytes(output)


def _signed_16_delta(first: int, second: int) -> int:
    """Return ``second - first`` as a signed 16-bit delta."""
    delta = (second - first) & WORD_MASK
    if delta > SIGNED_WORD_MAX:
        delta -= WORD_MODULUS
    return delta


def _count_backref_match(src: bytes, i: int, match_start: int, max_count: int) -> int:
    """Count matching bytes for a non-overlapping back-reference (no self-referential copies)."""
    n = len(src)
    dist = i - match_start
    max_count = min(max_count, dist)
    cnt = 0
    while cnt < max_count and i + cnt < n and src[match_start + cnt] == src[i + cnt]:
        cnt += 1
    return cnt


def _f5_shape_qualifies(
    src: bytes,
    start: int,
    fixed_byte: int,
    strict_prefix_pairs: int,
    pair_count: int,
) -> bool:
    """Return whether an F5 scan shape is accepted by the original encoder.

    ``strict_prefix_pairs`` is the number of leading pairs whose varying byte
    differs from ``fixed_byte``.  A "degenerate" pair is one whose varying byte
    equals the fixed byte.  The cases below were recovered from the 17-file
    reference corpus; they are encoder-policy rules, not requirements of the
    decompression format.
    """
    degenerate_pairs = sum(
        src[start + pair * 2 + 1] == fixed_byte
        for pair in range(strict_prefix_pairs, pair_count)
    )

    if strict_prefix_pairs >= 4:
        return True

    if strict_prefix_pairs == 1 and degenerate_pairs == 2:
        terminal_is_degenerate = src[start + (pair_count - 1) * 2 + 1] == fixed_byte
        if not (pair_count == 4 and terminal_is_degenerate):
            return True

    if strict_prefix_pairs in (2, 3):
        if degenerate_pairs == 1:
            return True
        if (
            strict_prefix_pairs == 3
            and degenerate_pairs == 2
            and pair_count >= F5_Q3_DEG2_MIN_PAIRS
        ):
            return True

    if pair_count == 5 and strict_prefix_pairs == 0:
        return True
    if strict_prefix_pairs == 1 and degenerate_pairs == 1:
        return True
    if pair_count == 5 and strict_prefix_pairs == 2 and degenerate_pairs == 2:
        return True

    if fixed_byte != 0 and pair_count == 4:
        if strict_prefix_pairs in (1, 2) and degenerate_pairs == 2:
            return True
        if strict_prefix_pairs == 0 and degenerate_pairs >= 1:
            return True
    if fixed_byte != 0 and strict_prefix_pairs == 0 and degenerate_pairs == 1:
        return True

    if (
        fixed_byte == 0
        and strict_prefix_pairs == 1
        and pair_count >= F5_ZERO_Q1_LONG_MIN_PAIRS
    ):
        return True
    if (
        strict_prefix_pairs == 2
        and degenerate_pairs == 2
        and pair_count >= F5_Q2_DEG2_MIN_PAIRS
    ):
        return True
    if (
        fixed_byte == 0
        and strict_prefix_pairs == 3
        and pair_count >= F5_ZERO_Q3_LONG_MIN_PAIRS
    ):
        return True

    return False


def _fb_qualifies_at(src: bytes, pos: int, n: int) -> bool:
    """Return whether an FB 16-bit arithmetic run starts at ``pos``."""
    return _scan_fb(src, pos, n) is not None


def _f5_suppressed_by_fe_cycle(
    src: bytes,
    start: int,
    n: int,
    pair_count: int,
    strict_prefix_pairs: int,
) -> bool:
    """Return whether a 3-byte FE cycle suppresses this F5 candidate.

    The observed GOLEM/GOSUB form has exactly one trailing degenerate pair.
    Its distance equals the byte span of the preceding strict F5 pairs, and
    the compact FE match is exactly three bytes long. A longer FE match follows
    normal LZ selection and therefore does not trigger this special suppression.
    """
    if pair_count < F5_MIN_PAIRS or strict_prefix_pairs != pair_count - 1:
        return False

    desired_distance = 2 * strict_prefix_pairs
    if (
        desired_distance < FE_DISTANCE_STEP
        or desired_distance > FE_MAX_DISTANCE
        or desired_distance % FE_DISTANCE_STEP
        or desired_distance > start
    ):
        return False

    match_len = 0
    while (
        match_len < FE_MAX_MATCH
        and start + match_len < n
        and src[start - desired_distance + match_len] == src[start + match_len]
    ):
        match_len += 1
    if match_len != FE_MIN_MATCH:
        return False

    for distance in range(
        FE_DISTANCE_STEP, FE_MAX_DISTANCE + 1, FE_DISTANCE_STEP
    ):
        if distance > start or distance == desired_distance:
            continue
        other_len = 0
        while (
            other_len < FE_MAX_MATCH
            and start + other_len < n
            and src[start - distance + other_len] == src[start + other_len]
        ):
            other_len += 1
        if other_len > FE_MIN_MATCH:
            return False

    return True


def _f5_secondary_interrupt(
    src: bytes,
    start: int,
    n: int,
    pair_count: int,
    structural_strict_prefix_pairs: int,
) -> bool:
    """Recognize the secondary F5 stop used by structured zero tables.

    The six reference occurrences (three byte sequences duplicated in
    GOLEM/GOSUB) share a local FE-length-4 / 0x10 marker signature.
    """
    if (
        src[start] != 0
        or pair_count < F5_MIN_PAIRS
        or pair_count % F5_SECONDARY_PHASE_MODULUS != F5_SECONDARY_PHASE
    ):
        return False

    varying_pos = start + pair_count * 2 - 1
    if varying_pos - 2 < start or varying_pos + 3 >= n:
        return False
    if (
        src[varying_pos] != F5_SECONDARY_MARKER
        or src[varying_pos - 2] != F5_SECONDARY_MARKER
    ):
        return False

    best_match_len = 0
    best_distance = 0
    for distance in range(
        FE_DISTANCE_STEP, FE_MAX_DISTANCE + 1, FE_DISTANCE_STEP
    ):
        if distance > varying_pos:
            break
        match_len = 0
        while (
            match_len < FE_MAX_MATCH
            and varying_pos + match_len < n
            and src[varying_pos - distance + match_len]
            == src[varying_pos + match_len]
        ):
            match_len += 1
        if (
            match_len > best_match_len
            or (
                match_len == best_match_len
                and match_len
                and distance > best_distance
            )
        ):
            best_match_len = match_len
            best_distance = distance

    if best_match_len != FC_MIN_MATCH:
        return False

    remaining_strict_pairs = structural_strict_prefix_pairs - pair_count
    next_varying_pos = start + pair_count * 2 + 1
    next_varying = src[next_varying_pos] if next_varying_pos < n else -1
    return (
        remaining_strict_pairs < F5_SECONDARY_SUFFIX_LIMIT
        or (
            best_distance == FE_DISTANCE_STEP
            and next_varying >= F5_SECONDARY_NEXT_MIN
        )
    )


def _f5_varying_arithmetic_interrupt(
    src: bytes, start: int, n: int, pair_count: int
) -> bool:
    """Stop F5 before three strided varying bytes form an arithmetic run."""
    varying_pos = start + pair_count * 2 - 1
    if varying_pos < start + 1 or varying_pos + 4 >= n:
        return False

    first = src[varying_pos]
    second = src[varying_pos + 2]
    third = src[varying_pos + 4]
    return ((second - first) & BYTE_MASK) == ((third - second) & BYTE_MASK)


def _scan_f4(src: bytes, start: int, n: int) -> F4Scan | None:
    """Scan the F4 repeated-triplet structure, including constant-run quirk."""
    if start + 2 >= n:
        return None

    first, second, third = src[start], src[start + 1], src[start + 2]
    if first == second == third:
        run_length = 1
        while (
            run_length < F4_CONSTANT_SCAN_LIMIT
            and start + run_length < n
            and src[start + run_length] == first
        ):
            run_length += 1
        triple_count = min(F4_MAX_TRIPLES, (run_length - 2) // 3)
    else:
        triple_count = 1
        while (
            triple_count < F4_MAX_TRIPLES
            and start + triple_count * 3 + 2 < n
            and src[start + triple_count * 3] == first
            and src[start + triple_count * 3 + 1] == second
            and src[start + triple_count * 3 + 2] == third
        ):
            triple_count += 1

    if triple_count < F4_MIN_TRIPLES:
        return None
    return F4Scan(first, second, third, triple_count)


def _scan_f6_structure(src: bytes, start: int, n: int) -> F6Scan | None:
    """Scan F6 triples and apply the recovered F1-run interruption rule."""
    if start + 2 >= n:
        return None

    first_fixed, second_fixed = src[start], src[start + 1]
    triple_count = 1
    limit = (
        F6_ZERO_FIXED_PROBE_TRIPLES
        if first_fixed == second_fixed == 0
        else F6_MAX_TRIPLES
    )

    while (
        triple_count < limit
        and start + triple_count * 3 + 2 < n
        and src[start + triple_count * 3] == first_fixed
        and src[start + triple_count * 3 + 1] == second_fixed
    ):
        varying_pos = start + triple_count * 3 + 2
        repeated = 1
        while (
            repeated < F1_MIN_RUN
            and varying_pos + repeated < n
            and src[varying_pos + repeated] == src[varying_pos]
        ):
            repeated += 1
        if repeated >= F1_MIN_RUN:
            break
        triple_count += 1

    structure_ended = not (
        start + triple_count * 3 + 2 < n
        and src[start + triple_count * 3] == first_fixed
        and src[start + triple_count * 3 + 1] == second_fixed
    )
    if (
        first_fixed != second_fixed
        and triple_count < limit
        and structure_ended
        and src[start + (triple_count - 1) * 3 + 2] == first_fixed
        and start + triple_count * 3 < n
        and src[start + triple_count * 3] == first_fixed
    ):
        triple_count -= 1

    if triple_count < F6_MIN_TRIPLES:
        return None

    varying = bytes(
        src[start + triple * 3 + 2] for triple in range(triple_count)
    )
    return F6Scan(first_fixed, second_fixed, triple_count, varying)


def _f6_lookahead_allowed(scan: F6Scan) -> bool:
    """Qualification used by one-level pattern lookahead."""
    if scan.first_fixed != scan.second_fixed:
        return True
    degenerate = sum(value == scan.first_fixed for value in scan.varying)
    if scan.first_fixed == 0:
        return degenerate <= 1 and scan.varying[-1] != 0
    return degenerate < 3


def _f6_emission_allowed(scan: F6Scan) -> bool:
    """Qualification used when F6 is a real emission candidate."""
    if scan.first_fixed != scan.second_fixed:
        return True
    degenerate = sum(value == scan.first_fixed for value in scan.varying)
    if scan.first_fixed == 0:
        return degenerate <= 1 and scan.varying[-1] != 0
    return degenerate * 2 <= scan.triple_count


def _scan_f7(src: bytes, start: int, n: int) -> F7Scan | None:
    """Scan the F7 ``{fixed, fixed, fixed, varying}`` quad structure."""
    if n - start < FULL_PATTERN_PROBE_BYTES:
        return None

    first, second, third = src[start], src[start + 1], src[start + 2]
    if first == second == third == 0:
        return None

    quad_count = 1
    while (
        quad_count < F7_MAX_QUADS
        and start + quad_count * 4 + 3 < n
        and src[start + quad_count * 4] == first
        and src[start + quad_count * 4 + 1] == second
        and src[start + quad_count * 4 + 2] == third
    ):
        quad_count += 1

    if quad_count < F7_MIN_QUADS:
        return None
    varying = bytes(
        src[start + quad * 4 + 3] for quad in range(quad_count)
    )
    return F7Scan(first, second, third, quad_count, varying)


def _scan_fb(src: bytes, start: int, n: int) -> FBScan | None:
    """Scan an FB signed-8-bit delta run over little-endian 16-bit values."""
    if start + 3 >= n:
        return None

    first = src[start] | (src[start + 1] << 8)
    second = src[start + 2] | (src[start + 3] << 8)
    delta = _signed_16_delta(first, second)
    if not (SIGNED_BYTE_MIN <= delta <= SIGNED_BYTE_MAX):
        return None

    value = first
    pair_count = 0
    while pair_count < FB_MAX_PAIRS and start + pair_count * 2 + 1 < n:
        lo = src[start + pair_count * 2]
        hi = src[start + pair_count * 2 + 1]
        if lo != (value & BYTE_MASK) or hi != ((value >> 8) & BYTE_MASK):
            break
        value = (value + delta) & WORD_MASK
        pair_count += 1

    if pair_count < FB_MIN_PAIRS:
        return None
    return FBScan(first, delta, pair_count)


def _scan_f5(src: bytes, start: int, n: int) -> F5Scan | None:
    """Scan the F5 ``{fixed, varying}`` pair structure at ``start``.

    The structural run is first measured without regard to opcode competition.
    The effective scan length is then shortened by the compatibility stop rules
    recovered from the reference corpus (FB hand-off, FE signature, arithmetic
    progression in the varying stream, and the GOLEM table-tail form).
    """
    if start + 1 >= n:
        return None

    fixed_byte = src[start]
    structural_pairs = 0
    while (
        structural_pairs < F5_MAX_PAIRS
        and start + structural_pairs * 2 + 1 < n
        and src[start + structural_pairs * 2] == fixed_byte
    ):
        structural_pairs += 1

    strict_prefix_pairs = 0
    while (
        strict_prefix_pairs < structural_pairs
        and src[start + strict_prefix_pairs * 2 + 1] != fixed_byte
    ):
        strict_prefix_pairs += 1

    if (
        structural_pairs == 6
        and strict_prefix_pairs == 0
        and (fixed_byte & NIBBLE_MASK) == 0
    ):
        varying = [src[start + pair * 2 + 1] for pair in range(6)]
        degeneracy = [value == fixed_byte for value in varying]
        if (
            all((value & NIBBLE_MASK) == 0 for value in varying)
            and degeneracy == [True, False, True, False, True, False]
        ):
            structural_pairs = 5

    structural_stop = None
    if strict_prefix_pairs >= 4 and structural_pairs == strict_prefix_pairs + 4:
        tail = [
            src[start + pair * 2 + 1]
            for pair in range(strict_prefix_pairs, structural_pairs)
        ]
        if (
            tail[0] == fixed_byte
            and tail[1] != fixed_byte
            and tail[2] != fixed_byte
            and tail[3] == fixed_byte
        ):
            structural_stop = strict_prefix_pairs + 2

    scanned_pairs = 0
    while scanned_pairs < structural_pairs:
        scanned_pairs += 1
        varying_pos = start + scanned_pairs * 2 - 1
        if _fb_qualifies_at(src, varying_pos, n):
            break
        if _f5_secondary_interrupt(
            src, start, n, scanned_pairs, strict_prefix_pairs
        ):
            break
        if _f5_varying_arithmetic_interrupt(src, start, n, scanned_pairs):
            break
        if structural_stop is not None and scanned_pairs >= structural_stop:
            break

    scanned_strict_pairs = min(strict_prefix_pairs, scanned_pairs)
    structural_degenerate_pairs = sum(
        src[start + pair * 2 + 1] == fixed_byte
        for pair in range(strict_prefix_pairs, structural_pairs)
    )
    scanned_degenerate_pairs = sum(
        src[start + pair * 2 + 1] == fixed_byte
        for pair in range(scanned_strict_pairs, scanned_pairs)
    )

    return F5Scan(
        fixed_byte,
        structural_pairs,
        strict_prefix_pairs,
        scanned_pairs,
        scanned_strict_pairs,
        structural_degenerate_pairs,
        scanned_degenerate_pairs,
    )


def _f5_base_candidate_allowed(
    src: bytes, start: int, n: int, scan: F5Scan
) -> bool:
    """Apply F5 exclusions shared by lookahead and actual token emission."""
    if scan.scanned_pairs < F5_MIN_PAIRS:
        return False

    if (
        scan.fixed_byte == 0
        and scan.strict_prefix_pairs == 5
        and (scan.structural_degenerate_pairs & 1)
    ):
        return False

    if scan.strict_prefix_pairs == 4 and scan.structural_degenerate_pairs == 3:
        return False

    if (
        scan.fixed_byte == 0
        and scan.structural_pairs == 5
        and scan.strict_prefix_pairs == 4
        and scan.structural_degenerate_pairs == 1
    ):
        return False

    if scan.scanned_pairs == 4 and scan.fixed_byte != 0:
        if (
            src[start + 1] == scan.fixed_byte
            and src[start + 3]
            == ((scan.fixed_byte + NIBBLE_STEP) & BYTE_MASK)
            and src[start + 5] == scan.fixed_byte
            and src[start + 7] == ((scan.fixed_byte - 1) & BYTE_MASK)
        ):
            return False

    f3_start = start + 2
    if f3_start + 5 < n:
        first, second = src[f3_start], src[f3_start + 1]
        pair_count = 1
        while (
            pair_count < F3_MAX_PAIRS
            and f3_start + pair_count * 2 + 1 < n
            and src[f3_start + pair_count * 2] == first
            and src[f3_start + pair_count * 2 + 1] == second
        ):
            pair_count += 1
        if pair_count >= 3:
            return False

    if _f5_suppressed_by_fe_cycle(
        src, start, n, scan.scanned_pairs, scan.scanned_strict_pairs
    ):
        return False

    return True


def _has_short_lz_match_at(src: bytes, start: int, n: int) -> bool:
    """Return whether FE or FC can encode a match starting at ``start``."""
    for distance in range(
        FE_DISTANCE_STEP, FE_MAX_DISTANCE + 1, FE_DISTANCE_STEP
    ):
        if distance > start:
            break
        match_len = 0
        while (
            match_len < FE_MAX_MATCH
            and start + match_len < n
            and src[start - distance + match_len] == src[start + match_len]
        ):
            match_len += 1
        if match_len >= FE_MIN_MATCH:
            return True

    if start + FC_MIN_MATCH <= n:
        window_start = max(0, start - FC_WINDOW)
        needle = src[start : start + FC_MIN_MATCH]
        if src.find(needle, window_start, start) >= 0:
            return True

    return False


def _f5_emission_candidate_allowed(
    src: bytes,
    start: int,
    n: int,
    pending_literals: int,
    scan: F5Scan,
) -> bool:
    """Apply F5 rules that depend on the surrounding encoder state.

    The F5 wire format itself is simple; most complexity here comes from the
    original compressor's qualification and lazy-start policy.  These branches
    are intentionally retained because they are required for 34/34 byte-exact
    reproduction of the known BIN corpus.
    """
    if not _f5_base_candidate_allowed(src, start, n, scan):
        return False

    fixed_byte = scan.fixed_byte
    pair_count = scan.scanned_pairs
    strict_pairs = scan.scanned_strict_pairs
    degenerate_pairs = scan.scanned_degenerate_pairs
    structural_tail = scan.structural_pairs - pair_count

    if (
        pending_literals == 0
        and fixed_byte == 0
        and strict_pairs == 1
        and degenerate_pairs == 1
    ):
        return False

    short_zero_q1_tail = (
        fixed_byte == 0
        and strict_pairs == 1
        and degenerate_pairs >= 2
        and structural_tail <= F5_SHORT_TAIL_PAIRS
    )

    low_q_zero_near_end = (
        fixed_byte == 0
        and 1 <= strict_pairs <= 3
        and degenerate_pairs >= 2
        and structural_tail <= F5_NEAR_END_TAIL_PAIRS
    )

    if (
        low_q_zero_near_end
        and strict_pairs == 3
        and pair_count <= 7
        and start + 1 < n
        and _has_short_lz_match_at(src, start + 1, n)
    ):
        return False

    if (
        fixed_byte == 0
        and pair_count == 4
        and strict_pairs == 2
        and degenerate_pairs == 2
        and _backref_starts_at_second_strict_byte(src, start, n)
    ):
        return False

    if (
        low_q_zero_near_end
        and start + 3 < n
        and src[start + 1] <= NIBBLE_MASK
        and src[start + 2] == fixed_byte
        and src[start + 3] == src[start + 1]
    ):
        return False

    if _f5_shape_qualifies(
        src, start, fixed_byte, strict_pairs, pair_count
    ):
        return True
    if short_zero_q1_tail or low_q_zero_near_end:
        return True

    terminal_is_strict = src[start + pair_count * 2 - 1] != fixed_byte
    clean_boundary_or_short_tail = (
        pending_literals == 0
        or structural_tail <= F5_SHORT_TAIL_PAIRS
        or terminal_is_strict
    )
    if not clean_boundary_or_short_tail:
        return False

    nonzero_q2_deg3_exception = (
        fixed_byte != 0
        and pair_count == 5
        and strict_pairs == 2
        and scan.structural_degenerate_pairs == 3
    )
    return not nonzero_q2_deg3_exception


def _backref_starts_at_second_strict_byte(src: bytes, start: int, n: int) -> bool:
    """Return whether LZ takes over at the second varying byte of a short F5.

    A back-reference saving at least two bytes starts at ``start + 3`` and none
    starts one byte earlier.
    """
    earlier = _find_best_backref(src, start + 2, n)
    if earlier is not None:
        return False
    at_second = _find_best_backref(src, start + 3, n)
    return at_second is not None and at_second.savings >= 2


def _best_pattern_savings(src: bytes, i: int, n: int) -> int:
    """Compute the best single-step pattern savings at position i (no back-refs)."""
    if i >= n:
        return 0
    v0 = src[i]
    best = 0

    def chk(enc_len, adv):
        nonlocal best
        sav = adv - enc_len
        if sav > best:
            best = sav

    if v0 <= NIBBLE_MASK:
        cnt = 1
        while cnt < F0_MAX_RUN and i + cnt < n and src[i + cnt] == v0:
            cnt += 1
        if cnt >= F0_MIN_RUN:
            chk(F0_ENCODED_SIZE, cnt)

    cnt = 1
    while cnt < F1_MAX_RUN and i + cnt < n and src[i + cnt] == v0:
        cnt += 1
    if cnt >= F1_MIN_RUN:
        chk(F1_ENCODED_SIZE, cnt)

    if i + 1 < n:
        b0, b1 = src[i], src[i + 1]
        if b0 <= NIBBLE_MASK and b1 <= NIBBLE_MASK:
            cnt = 1
            while (
                cnt < F2_MAX_PAIRS
                and i + cnt * 2 + 1 < n
                and src[i + cnt * 2] == b0
                and src[i + cnt * 2 + 1] == b1
            ):
                cnt += 1
            if cnt >= F2_MIN_PAIRS:
                chk(F2_ENCODED_SIZE, cnt * 2)

    if i + 1 < n:
        b0, b1 = src[i], src[i + 1]
        cnt = 1
        while (
            cnt < F3_MAX_PAIRS
            and i + cnt * 2 + 1 < n
            and src[i + cnt * 2] == b0
            and src[i + cnt * 2 + 1] == b1
        ):
            cnt += 1
        if cnt >= F3_LOOKAHEAD_MIN_PAIRS:
            chk(F3_ENCODED_SIZE, cnt * 2)

    f4_scan = _scan_f4(src, i, n)
    if f4_scan is not None:
        chk(F4_ENCODED_SIZE, f4_scan.triple_count * 3)

    f5_scan = _scan_f5(src, i, n)
    if (
        f5_scan is not None
        and _f5_base_candidate_allowed(src, i, n, f5_scan)
        and _f5_shape_qualifies(
            src,
            i,
            f5_scan.fixed_byte,
            f5_scan.scanned_strict_pairs,
            f5_scan.scanned_pairs,
        )
    ):
        chk(F5_BASE_ENCODED_SIZE + f5_scan.scanned_pairs, f5_scan.scanned_pairs * 2)

    f6_scan = _scan_f6_structure(src, i, n)
    if f6_scan is not None and _f6_lookahead_allowed(f6_scan):
        chk(
            F6_BASE_ENCODED_SIZE + f6_scan.triple_count,
            f6_scan.triple_count * 3,
        )

    f7_scan = _scan_f7(src, i, n)
    if f7_scan is not None:
        chk(
            F7_BASE_ENCODED_SIZE + f7_scan.quad_count,
            f7_scan.quad_count * 4,
        )

    cnt = 1
    while cnt < F8_MAX_RUN and i + cnt < n and src[i + cnt] == (v0 + cnt) & BYTE_MASK:
        cnt += 1
    if cnt >= F8_MIN_RUN:
        chk(F8_ENCODED_SIZE, cnt)

    cnt = 1
    while cnt < F9_MAX_RUN and i + cnt < n and src[i + cnt] == (v0 - cnt) & BYTE_MASK:
        cnt += 1
    if cnt >= F9_MIN_RUN:
        chk(F9_ENCODED_SIZE, cnt)

    if i + 1 < n:
        step = (src[i + 1] - v0) & BYTE_MASK
        cnt = 1
        while cnt < FA_MAX_RUN and i + cnt < n and src[i + cnt] == (v0 + step * cnt) & BYTE_MASK:
            cnt += 1
        if cnt >= FA_MIN_RUN:
            chk(FA_ENCODED_SIZE, cnt)

    fb_scan = _scan_fb(src, i, n)
    if fb_scan is not None:
        chk(FB_ENCODED_SIZE, fb_scan.pair_count * 2)

    return best


def _find_best_backref(src: bytes, start: int, n: int) -> BackrefCandidate | None:
    """Return the best FE/FD/FC candidate at ``start``.

    Equal-savings matches use the recovered original tie-break: prefer a match
    source with the same byte parity as ``start``; within that parity prefer the
    oldest/largest-distance match, then the longer advance.
    """
    first_byte = src[start]
    best: BackrefCandidate | None = None

    def consider(encoded, advance, match_start):
        nonlocal best

        candidate = BackrefCandidate(
            encoded=bytes(encoded),
            advance=advance,
            savings=advance - len(encoded),
            distance=start - match_start,
            same_parity=((match_start ^ start) & 1) == 0,
        )

        if best is None:
            best = candidate
            return
        if candidate.savings > best.savings:
            best = candidate
            return
        if candidate.savings != best.savings:
            return
        if candidate.same_parity and not best.same_parity:
            best = candidate
            return
        if candidate.same_parity != best.same_parity:
            return
        if candidate.distance > best.distance:
            best = candidate
            return
        if candidate.distance == best.distance and candidate.advance > best.advance:
            best = candidate

    for distance in range(
        FE_MAX_DISTANCE, FE_DISTANCE_STEP - 1, -FE_DISTANCE_STEP
    ):
        if distance > start:
            continue
        match_start = start - distance
        if src[match_start] != first_byte:
            continue

        match_len = _count_backref_match(
            src, start, match_start, FE_MAX_MATCH
        )
        if match_len < FE_MIN_MATCH:
            continue

        upper_nibble = distance // FE_DISTANCE_STEP - 1
        packed = (upper_nibble << 4) | (match_len - FE_MIN_MATCH)
        consider([OP_FE, packed], match_len, match_start)

    have_fd = False
    if start >= 1 and start + FC_MIN_MATCH <= n:
        key = src[start : start + FC_MIN_MATCH]
        window_start = max(0, start - FD_WINDOW)
        match_start = src.find(key, window_start, start)

        while match_start >= 0:
            distance = start - match_start
            if distance >= FD_MIN_MATCH:
                match_len = _count_backref_match(
                    src, start, match_start, FD_MAX_MATCH
                )
                if match_len >= FD_MIN_MATCH:
                    match_len = min(match_len, FD_MAX_MATCH)
                    consider(
                        [
                            OP_FD,
                            distance - 1,
                            match_len - FD_MIN_MATCH,
                        ],
                        match_len,
                        match_start,
                    )
                    have_fd = True
            match_start = src.find(key, match_start + 1, start)

        if not have_fd:
            window_start = max(0, start - FC_WINDOW)
            max_match = min(FC_MAX_MATCH, n - start)
            for match_len in range(max_match, FC_MIN_MATCH - 1, -1):
                needle = src[start : start + match_len]
                first_match = src.find(needle, window_start, start)
                if first_match < 0:
                    continue

                chosen = first_match
                if ((first_match ^ start) & 1) != 0:
                    alternative = src.find(needle, first_match + 1, start)
                    while alternative >= 0:
                        if ((alternative ^ start) & 1) == 0:
                            chosen = alternative
                            break
                        alternative = src.find(needle, alternative + 1, start)

                offset = start - chosen - 1
                consider(
                    [
                        OP_FC,
                        offset & BYTE_MASK,
                        ((match_len - FC_MIN_MATCH) << 4)
                        | ((offset >> 8) & NIBBLE_MASK),
                    ],
                    match_len,
                    chosen,
                )
                break

    return best


def _select_encoding(
    src: bytes, i: int, n: int, pending_literals: int = 0
) -> tuple[bytes, int] | None:
    """Choose the original compressor's token at source position ``i``.

    Selection is intentionally not a generic "best compression" decision:

    * F0-FB pattern candidates are filtered by recovered opcode precedence and
      normally scored with one level of pattern-only lookahead.
    * FE/FD/FC candidates are ranked by savings, then same-byte-parity source,
      then oldest distance, matching the observed LZ tie-break behavior.
    * Pattern-vs-LZ efficiency is compared in 12-bit fixed point; a quantized
      tie belongs to the pattern candidate.

    ``pending_literals`` is the number of currently buffered literal bytes.
    Some F3/F5 qualifications depend on whether the encoder is at a clean
    token boundary.
    Returns ``None`` when the next source byte should remain literal.
    """
    first_byte = src[i]

    pattern_candidates: list[PatternCandidate] = []

    def add_pattern_candidate(encoded, advance, count):
        savings = advance - len(encoded)
        if savings >= 0:
            pattern_candidates.append(
                PatternCandidate(bytes(encoded), advance, savings, count)
            )

    best_pattern: PatternCandidate | None = None
    best_pattern_score = 0

    if first_byte <= NIBBLE_MASK:
        cnt = 1
        while cnt < F0_MAX_RUN and i + cnt < n and src[i + cnt] == first_byte:
            cnt += 1
        if cnt >= F0_MIN_RUN:
            add_pattern_candidate([OP_F0, (first_byte << 4) | (cnt - F0_MIN_RUN)], cnt, cnt)

    cnt = 1
    while cnt < F1_MAX_RUN and i + cnt < n and src[i + cnt] == first_byte:
        cnt += 1
    if cnt >= F1_MIN_RUN:
        add_pattern_candidate([OP_F1, cnt - F1_MIN_RUN, first_byte], cnt, cnt)

    if i + 1 < n:
        b0, b1 = src[i], src[i + 1]
        if b0 <= NIBBLE_MASK and b1 <= NIBBLE_MASK:
            cnt = 1
            while (cnt < F2_MAX_PAIRS and i + cnt * 2 + 1 < n
                   and src[i + cnt * 2] == b0 and src[i + cnt * 2 + 1] == b1):
                cnt += 1
            if cnt >= F2_MIN_PAIRS:
                add_pattern_candidate([OP_F2, cnt - F2_MIN_PAIRS, (b1 << 4) | b0], cnt * 2, cnt)

    if i + 1 < n:
        b0, b1 = src[i], src[i + 1]
        cnt = 1
        while (cnt < F3_MAX_PAIRS and i + cnt * 2 + 1 < n
               and src[i + cnt * 2] == b0 and src[i + cnt * 2 + 1] == b1):
            cnt += 1
        if cnt >= F3_MIN_PAIRS:
            add_pattern_candidate([OP_F3, cnt - F3_MIN_PAIRS, b0, b1], cnt * 2, cnt)

    f4_scan = _scan_f4(src, i, n) if n - i >= FULL_PATTERN_PROBE_BYTES else None
    if f4_scan is not None:
        add_pattern_candidate(
            [
                OP_F4,
                f4_scan.triple_count - F4_MIN_TRIPLES,
                f4_scan.first,
                f4_scan.second,
                f4_scan.third,
            ],
            f4_scan.triple_count * 3,
            f4_scan.triple_count,
        )

    f5_scan = _scan_f5(src, i, n)
    if (
        f5_scan is not None
        and _f5_emission_candidate_allowed(src, i, n, pending_literals, f5_scan)
    ):
        varying = bytes(
            src[i + pair * 2 + 1] for pair in range(f5_scan.scanned_pairs)
        )
        encoded = bytes(
            [OP_F5, f5_scan.scanned_pairs - F5_MIN_PAIRS, f5_scan.fixed_byte]
        ) + varying
        add_pattern_candidate(
            encoded, f5_scan.scanned_pairs * 2, f5_scan.scanned_pairs
        )

    f6_scan = _scan_f6_structure(src, i, n)
    if f6_scan is not None and _f6_emission_allowed(f6_scan):
        encoded = bytes(
            [
                OP_F6,
                f6_scan.triple_count - F6_MIN_TRIPLES,
                f6_scan.first_fixed,
                f6_scan.second_fixed,
            ]
        ) + f6_scan.varying
        add_pattern_candidate(
            encoded, f6_scan.triple_count * 3, f6_scan.triple_count
        )

    f7_scan = _scan_f7(src, i, n)
    if f7_scan is not None:
        encoded = bytes(
            [
                OP_F7,
                f7_scan.quad_count - F7_MIN_QUADS,
                f7_scan.first_fixed,
                f7_scan.second_fixed,
                f7_scan.third_fixed,
            ]
        ) + f7_scan.varying
        add_pattern_candidate(
            encoded, f7_scan.quad_count * 4, f7_scan.quad_count
        )

    cnt = 1
    while cnt < F8_MAX_RUN and i + cnt < n and src[i + cnt] == (first_byte + cnt) & BYTE_MASK:
        cnt += 1
    if cnt >= F8_MIN_RUN:
        add_pattern_candidate([OP_F8, cnt - F8_MIN_RUN, first_byte], cnt, cnt)

    cnt = 1
    while cnt < F9_MAX_RUN and i + cnt < n and src[i + cnt] == (first_byte - cnt) & BYTE_MASK:
        cnt += 1
    if cnt >= F9_MIN_RUN:
        add_pattern_candidate([OP_F9, cnt - F9_MIN_RUN, first_byte], cnt, cnt)

    if i + 1 < n:
        step = (src[i + 1] - first_byte) & BYTE_MASK
        cnt = 1
        while (
            cnt < FA_MAX_RUN
            and i + cnt < n
            and src[i + cnt] == (first_byte + step * cnt) & BYTE_MASK
        ):
            cnt += 1
        if cnt >= FA_MIN_RUN:
            add_pattern_candidate([OP_FA, cnt - FA_MIN_RUN, first_byte, step], cnt, cnt)

    fb_scan = _scan_fb(src, i, n)
    if fb_scan is not None:
        add_pattern_candidate(
            [
                OP_FB,
                fb_scan.pair_count - FB_MIN_PAIRS,
                src[i],
                src[i + 1],
                fb_scan.delta & BYTE_MASK,
            ],
            fb_scan.pair_count * 2,
            fb_scan.pair_count,
        )

    best_backref = _find_best_backref(src, i, n)

    if best_backref is not None and f5_scan is not None:
        filtered: list[PatternCandidate] = []
        for candidate in pattern_candidates:
            is_short_zero_q1_f5 = (
                candidate.opcode == OP_F5
                and f5_scan.fixed_byte == 0
                and f5_scan.scanned_strict_pairs == 1
                and f5_scan.scanned_degenerate_pairs >= 2
                and f5_scan.structural_pairs - f5_scan.scanned_pairs
                <= F5_SHORT_TAIL_PAIRS
                and f5_scan.scanned_pairs <= F5_SHORT_LZ_PREEMPT_MAX_PAIRS
            )
            if not is_short_zero_q1_f5:
                filtered.append(candidate)
        pattern_candidates = filtered

    max_fa = next((c for c in pattern_candidates
                   if c.opcode == OP_FA and c.advance == FA_MAX_RUN and c.encoded[3] != 0), None)
    if max_fa is not None:
        pattern_candidates = [max_fa]

    fa_candidate = next((c for c in pattern_candidates if c.opcode == OP_FA), None)
    if fa_candidate is not None:
        pattern_candidates = [
            c
            for c in pattern_candidates
            if not (
                (c.opcode == OP_F3 or (c.opcode == OP_FB and c.encoded[4] == 0))
                and c.advance <= fa_candidate.advance
            )
        ]

    has_f5 = any(c.opcode == OP_F5 for c in pattern_candidates)
    if has_f5:
        pattern_candidates = [c for c in pattern_candidates if c.opcode != OP_F7]

    if any(c.opcode == OP_F0 for c in pattern_candidates):
        pattern_candidates = [
            c
            for c in pattern_candidates
            if c.opcode not in (OP_F5, OP_F6, OP_F7)
        ]

    if any(c.opcode == OP_F2 for c in pattern_candidates):
        pattern_candidates = [
            c
            for c in pattern_candidates
            if c.opcode not in (OP_F5, OP_F6, OP_F7)
        ]

    if any(c.opcode == OP_F4 for c in pattern_candidates):
        pattern_candidates = [c for c in pattern_candidates if c.opcode != OP_F6]

    if any(c.opcode == OP_F6 for c in pattern_candidates):
        j4 = i + 3
        lazy_f4 = (
            _scan_f4(src, j4, n)
            if n - j4 >= FULL_PATTERN_PROBE_BYTES
            else None
        )
        if lazy_f4 is not None:
            pattern_candidates = [c for c in pattern_candidates if c.opcode != OP_F6]

    long_const_f4 = any(c.opcode == OP_F4 and c.advance >= LONG_CONSTANT_F4_MIN_ADVANCE
                        and src[i] == src[i + 1] == src[i + 2]
                        for c in pattern_candidates)
    if long_const_f4:
        pattern_candidates = [c for c in pattern_candidates if c.opcode != OP_F2]

    eligible_f3 = any(c.opcode == OP_F3 and (c.savings > 0 or pending_literals == 0)
                      for c in pattern_candidates)
    if eligible_f3:
        pattern_candidates = [
            c
            for c in pattern_candidates
            if c.opcode not in (OP_F5, OP_F6, OP_F7)
        ]

    byte_run = 1
    while (
        byte_run < F1_MIN_RUN
        and i + byte_run < n
        and src[i + byte_run] == first_byte
    ):
        byte_run += 1
    constant_rle = byte_run >= F1_MIN_RUN

    if constant_rle:
        for candidate in pattern_candidates:
            if candidate.opcode not in (OP_F0, OP_F1, OP_F2, OP_F4):
                continue
            if candidate.savings <= 0:
                continue
            if (
                best_pattern is None
                or candidate.savings > best_pattern_score
                or (
                    candidate.savings == best_pattern_score
                    and candidate.count > best_pattern.count
                )
            ):
                best_pattern = candidate
                best_pattern_score = candidate.savings
    else:
        for candidate in pattern_candidates:
            lookahead_score = candidate.savings + _best_pattern_savings(
                src, i + candidate.advance, n
            )

            if (
                candidate.savings == 0
                and candidate.opcode == OP_F3
                and pending_literals != 0
            ):
                continue
            if lookahead_score <= 0 and not (
                lookahead_score == 0
                and candidate.opcode == OP_F3
                and pending_literals == 0
            ):
                continue

            if (
                best_pattern is None
                or lookahead_score > best_pattern_score
                or (
                    lookahead_score == best_pattern_score
                    and candidate.count > best_pattern.count
                )
            ):
                best_pattern = candidate
                best_pattern_score = lookahead_score

    if best_backref is not None and best_pattern is not None:
        pattern_efficiency = (
            best_pattern.savings << EFFICIENCY_FRACTION_BITS
        ) // best_pattern.advance
        backref_efficiency = (
            best_backref.savings << EFFICIENCY_FRACTION_BITS
        ) // best_backref.advance

        if backref_efficiency > pattern_efficiency:
            for rescue_opcode, require_strict_win in (
                (OP_F1, True),
                (OP_F0, False),
            ):
                for candidate in pattern_candidates:
                    if candidate.opcode != rescue_opcode:
                        continue
                    ratio_delta = (
                        candidate.savings * best_backref.advance
                        - best_backref.savings * candidate.advance
                    )
                    if require_strict_win and ratio_delta > 0:
                        return candidate.encoded, candidate.advance
                    if (
                        not require_strict_win
                        and ratio_delta == 0
                        and best_backref.opcode != OP_FD
                    ):
                        return candidate.encoded, candidate.advance
            return best_backref.encoded, best_backref.advance

        return best_pattern.encoded, best_pattern.advance

    if best_pattern is not None:
        return best_pattern.encoded, best_pattern.advance
    if best_backref is not None:
        return best_backref.encoded, best_backref.advance
    return None


def compress_file(input_path: str | Path, output_path: str | Path) -> tuple[int, int]:
    """Compress one file and return ``(input_size, output_size)``."""
    input_path = Path(input_path)
    output_path = Path(output_path)

    source = input_path.read_bytes()
    compressed = compress(source)
    output_path.write_bytes(compressed)
    return len(source), len(compressed)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Compress a Legend of Mana BIN stream byte-exactly."
    )
    parser.add_argument("input", help="decompressed input file")
    parser.add_argument("output", help="compressed output file")
    args = parser.parse_args(argv)

    input_size, output_size = compress_file(args.input, args.output)
    ratio = output_size / input_size if input_size else 0.0
    print(
        f"Compressed {input_size} bytes -> {output_size} bytes "
        f"(ratio: {ratio:.3f})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
