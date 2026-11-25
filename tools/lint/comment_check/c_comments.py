from __future__ import annotations

import re

from .common import Finding, is_license, line_of, line_starts
from .settings import WHITESPACE


NOT_FUNCTION_NAMES = {
    "sizeof", "alignof", "_Alignof", "decltype", "if", "while", "for", "switch", "return", "_Static_assert",
    "static_assert", "__attribute__", "__declspec", "alignas", "_Alignas", "defined", "__typeof__", "typeof", "_Atomic",
}

PURE_OR_DEFAULTED = re.compile(r"\)\s*(?:const|override|final|noexcept|volatile|&|\s)*$")

DECLARATION_NOISE = re.compile(r"\b(__attribute__|__declspec|alignas|_Alignas)\s*\(")

TEMPLATE_PREFIX = re.compile(r"^\s*template\s*<")

SCOPE_OPENER = re.compile(r"\b(namespace|struct|class|union)\b[^()=]*$", re.S)

EXTERN_BLOCK = re.compile(r'\bextern\s*"[^"]*"\s*$')

ENUM_OPENER = re.compile(r"\benum\b[^()=]*$", re.S)

CONDITIONAL_DIRECTIVE = re.compile(r"#\s*(if|ifdef|ifndef)\b")


def skip_c_literal(text, start, masked):
    """Step over a C or C++ string or character literal, blanking its contents.

    A quote with no closing quote on its line is taken as a stray character
    (an apostrophe in an #error, say), not a literal.

    @param text The source.
    @param start The offset of the opening quote.
    @param masked The masked copy of the source, changed in place.
    @return The offset just past the literal.
    """
    quote = text[start]
    raw = re.search(r'(?:\bu8|\bu|\bU|\bL)?R$', text[max(0, start - 3):start])
    if quote == '"' and raw:
        open_paren = text.find("(", start)
        delimiter = text[start + 1:open_paren]
        close = text.find(")" + delimiter + '"', open_paren)
        if open_paren >= 0 and close >= 0:
            end = close + len(delimiter) + 2
            for index in range(start + 1, end - 1):
                if masked[index] != "\n":
                    masked[index] = "_"
            return end
    index = start + 1
    while index < len(text):
        character = text[index]
        if character == "\\":
            index += 2
            continue
        if character == quote:
            for inner in range(start + 1, index):
                masked[inner] = "_"
            return index + 1
        if character == "\n":
            return start + 1
        index += 1
    return start + 1


def find_c_comments(text):
    """Find every comment in C-like source, and blank out comments and literals.

    @param text The source.
    @return The comments as (start, end) offsets, and the source with comments
            turned to spaces and literal contents to underscores, so braces and
            semicolons in them don't count.
    """
    masked = list(text)
    comments = []
    index = 0
    length = len(text)
    while index < length:
        if text.startswith("//", index):
            end = index
            while True:
                newline = text.find("\n", end)
                if newline < 0:
                    end = length
                    break
                if text[newline - 1] == "\\":
                    end = newline + 1
                    continue
                end = newline
                break
            comments.append((index, end))
            index = end
        elif text.startswith("/*", index):
            close = text.find("*/", index + 2)
            end = length if close < 0 else close + 2
            comments.append((index, end))
            index = end
        elif text[index] in "\"'":
            index = skip_c_literal(text, index, masked)
        else:
            index += 1
    for start, end in comments:
        for inner in range(start, end):
            if masked[inner] != "\n":
                masked[inner] = " "
    return comments, "".join(masked)


def blank_directives(masked):
    """Blank out preprocessor lines, so their braces don't count as scopes.

    @param masked Source from find_c_comments.
    @return The same source with every preprocessor line, continuations
            included, turned to spaces.
    """
    lines = masked.split("\n")
    in_directive = False
    for number, line in enumerate(lines):
        if in_directive or line.lstrip().startswith("#"):
            in_directive = line.rstrip().endswith("\\")
            lines[number] = " " * len(line)
    return "\n".join(lines)


def classify_brace(statement, parent):
    """Decide what a '{' opens, from the code before it.

    @param statement The masked code since the last ';', '{' or '}'.
    @param parent What the enclosing brace opened.
    @return "scope" for a namespace, extern "C", struct, class or union body;
            "enum" or "init" for those; "body" for anything with statements in
            it.
    """
    if parent in ("body", "init", "enum"):
        return "body" if parent == "body" else parent
    text = statement.strip()
    if EXTERN_BLOCK.search(text) or SCOPE_OPENER.search(text):
        return "scope"
    if ENUM_OPENER.search(text):
        return "enum"
    if "=" in text.replace("==", "") and not re.search(r"\)\s*(const\s*)?$", text):
        return "init"
    return "body"


def scope_map(masked):
    """Work out the kind of scope every offset of the source is in.

    @param masked Source from blank_directives.
    @return A list, per offset, of "file", "scope", "enum", "init" or "body".
    """
    kinds = []
    stack = ["file"]
    statement_start = 0
    for index, character in enumerate(masked):
        kinds.append(stack[-1])
        if character == "{":
            stack.append(classify_brace(masked[statement_start:index], stack[-1]))
            statement_start = index + 1
        elif character == "}":
            if len(stack) > 1:
                stack.pop()
            statement_start = index + 1
        elif character == ";":
            statement_start = index + 1
    return kinds


def strip_balanced(text, opener_end):
    """Remove a parenthesized group that starts just before opener_end.

    @param text The text.
    @param opener_end The offset just past the group's '('.
    @return The text with the group's contents and closing ')' removed.
    """
    depth = 1
    index = opener_end
    while index < len(text) and depth:
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
        index += 1
    return text[:opener_end - 1] + " " + text[index:]


def declaration_after(masked, start):
    """Read the declaration that starts at an offset, up to its body, ';' or '='.

    @param masked Source from find_c_comments.
    @param start Where the declaration starts.
    @return The declaration's text, with attributes, alignment and a template
            prefix taken out, and the character that ended it.
    """
    depth = 0
    index = start
    while index < len(masked):
        character = masked[index]
        if character in "([":
            depth += 1
        elif character in ")]":
            depth -= 1
        elif depth == 0 and character in ";{=":
            break
        index += 1
    text = masked[start:index]
    terminator = masked[index] if index < len(masked) else ""
    while True:
        match = DECLARATION_NOISE.search(text)
        if not match:
            break
        text = strip_balanced(text, match.end())
    text = re.sub(r"\[\[.*?\]\]", " ", text)
    if TEMPLATE_PREFIX.match(text):
        depth = 0
        for index, character in enumerate(text):
            if character == "<":
                depth += 1
            elif character == ">":
                depth -= 1
                if depth == 0:
                    text = text[index + 1:]
                    break
    return text, terminator


def is_function(declaration, terminator):
    """Decide whether a declaration declares or defines a function.

    @param declaration Text from declaration_after.
    @param terminator The character that ended it.
    @return True for a function, method, constructor or operator; False for a
            variable, function pointer, type or anything else.
    """
    text = declaration.strip()
    if not text or re.match(r"(typedef|using)\b", text):
        return False
    if terminator == "=" and not PURE_OR_DEFAULTED.search(text):
        return False
    paren = text.find("(")
    if paren < 0:
        return False
    before = text[:paren].rstrip()
    after = text[paren + 1:].lstrip()
    if after.startswith(("*", "^", "&")) and not re.search(r"\boperator\b", before):
        return False
    if re.search(r"\boperator\b", before):
        return True
    name = re.search(r"(~?[A-Za-z_]\w*)$", before)
    return bool(name) and name.group(1) not in NOT_FUNCTION_NAMES


def is_doc_comment(text):
    """Check whether a C comment is written as a doc block.

    @param text The comment.
    @return True for /** ... */ and /// comments, but not the /**< and ///<
            forms that document what comes before them.
    """
    if text.startswith("/**<") or text.startswith("///<"):
        return False
    return (text.startswith("/**") and text != "/**/") or text.startswith("///") or text.startswith("/*!")


def group_doc_runs(text, comments):
    """Join each run of /// lines into one comment.

    @param text The source.
    @param comments The comments from find_c_comments.
    @return The comments, with each run of /// comments separated only by
            whitespace merged into one (start, end).
    """
    grouped = []
    for start, end in comments:
        if (grouped and text[start:start + 3] == "///" and text[grouped[-1][0]:grouped[-1][0] + 3] == "///"
                and not text[grouped[-1][1]:start].strip() and text[grouped[-1][1]:start].count("\n") == 1):
            grouped[-1] = (grouped[-1][0], end)
        else:
            grouped.append((start, end))
    return grouped


def skip_conditionals(text, offset):
    """Step over #if, #ifdef and #ifndef lines, so a doc block above a function
    that comes in versions (one per game region, say) still counts as on it.

    @param text The source.
    @param offset Where the code after a doc block starts.
    @return Where the first line that isn't one of those starts.
    """
    while CONDITIONAL_DIRECTIVE.match(text, offset):
        newline = text.find("\n", offset)
        if newline < 0:
            return len(text)
        offset = WHITESPACE.match(text, newline).end()
    return offset


def scan_c(path, text):
    """Find the comments in C-like source that the rule doesn't allow.

    @param path The file's path, for the findings.
    @param text The source.
    @return The findings.
    """
    comments, masked = find_c_comments(text)
    comments = group_doc_runs(text, comments)
    directive_free = blank_directives(masked)
    kinds = scope_map(directive_free)
    starts = line_starts(text)
    findings = []
    for start, end in comments:
        comment = text[start:end]
        if is_license(comment):
            continue
        following = WHITESPACE.match(text, end).end()
        newline = text.find("\n", following)
        context = text[following:len(text) if newline < 0 else newline].strip()
        if is_doc_comment(comment):
            target = skip_conditionals(text, following)
            if (kinds[start] in ("file", "scope") and target < len(text) and not text.startswith(("/", "#"), target)
                    and is_function(*declaration_after(masked, target))):
                continue
            kind = "doc-not-on-function"
        else:
            kind = "comment"
        findings.append(Finding(path, line_of(starts, start), start, end, kind, comment, context))
    return findings
