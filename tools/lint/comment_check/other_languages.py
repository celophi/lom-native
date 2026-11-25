from __future__ import annotations

import ast
import io
import re
import tokenize

from .common import Finding, is_license, line_of, line_starts


YAML_BLOCK_SCALAR = re.compile(r"^(\s*(?:-\s+)?)([\w.-]+):\s*[|>][-+0-9]*\s*(#.*)?$")

SHELL_HEREDOC = re.compile(r"<<(-?)\s*(['\"]?)([A-Za-z_][\w]*)\2")

SHELL_COMMENT_LEADERS = " \t;|&("


def python_spans(text):
    """Map Python's line and column positions to character offsets.

    @param text The source.
    @return Two functions taking (line, column) and returning the offset: one
            for tokenize's columns, in characters, and one for ast's, in UTF-8
            bytes.
    """
    starts = line_starts(text)
    lines = text.split("\n")

    def offset(line, column):
        """Turn a 1-based line and a column in characters into an offset.

        @param line The line.
        @param column The column, in characters.
        @return The offset.
        """
        return starts[line - 1] + column

    def byte_offset(line, column):
        """Turn a 1-based line and a column in UTF-8 bytes into an offset.

        @param line The line.
        @param column The column, in bytes.
        @return The offset.
        """
        return starts[line - 1] + len(lines[line - 1].encode("utf-8")[:column].decode("utf-8", "ignore"))

    return offset, byte_offset


def statement_bodies(tree):
    """List every statement list in a Python syntax tree, with its owner.

    @param tree The parsed module.
    @return (owner node, attribute name, statements) for every body, orelse,
            finalbody and handler body.
    """
    bodies = []
    for node in ast.walk(tree):
        for attribute in ("body", "orelse", "finalbody"):
            statements = getattr(node, attribute, None)
            if isinstance(statements, list) and statements and isinstance(statements[0], ast.stmt):
                bodies.append((node, attribute, statements))
    return bodies


def scan_python(path, text):
    """Find the comments and stray strings in Python source that the rule doesn't allow.

    @param path The file's path, for the findings.
    @param text The source.
    @return The findings. A module docstring the code reads through __doc__ is
            reported as not fixable, since taking it out would change the
            program.
    """
    offset, byte_offset = python_spans(text)
    findings = []
    lines = text.split("\n")
    for token in tokenize.generate_tokens(io.StringIO(text).readline):
        if token.type != tokenize.COMMENT:
            continue
        if token.start[0] == 1 and token.string.startswith("#!"):
            continue
        if is_license(token.string):
            continue
        start = offset(*token.start)
        findings.append(Finding(path, token.start[0], start, start + len(token.string), "comment", token.string,
                                lines[token.start[0] - 1].strip()))
    tree = ast.parse(text)
    reads_doc = "__doc__" in text
    for owner, attribute, statements in statement_bodies(tree):
        for position, statement in enumerate(statements):
            if not (isinstance(statement, ast.Expr) and isinstance(statement.value, ast.Constant)
                    and isinstance(statement.value.value, str)):
                continue
            if position == 0 and attribute == "body" and isinstance(owner, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            if is_license(statement.value.value):
                continue
            module_doc = isinstance(owner, ast.Module) and position == 0
            kind = "module-docstring" if module_doc else "class-docstring" if position == 0 and isinstance(owner, ast.ClassDef) else "bare-string"
            start = byte_offset(statement.lineno, statement.col_offset)
            end = byte_offset(statement.end_lineno, statement.end_col_offset)
            fixable = not (module_doc and reads_doc)
            replacement = "" if isinstance(owner, ast.Module) or len(statements) > 1 else "pass"
            findings.append(Finding(path, statement.lineno, start, end, kind, text[start:end], "", fixable, replacement))
    return findings


def shell_comments(text, base=0, allow_shebang=True):
    """Find the comments in shell script text.

    Tracks quotes across lines and skips here-documents. A '#' starts a comment
    at the start of a word, not in $# or ${#name}.

    @param text The script.
    @param base Added to every offset, for script text inside another file.
    @param allow_shebang Whether a #! first line is allowed.
    @return The comments as (start, end) offsets.
    """
    comments = []
    quote = None
    heredoc = None
    pending_heredoc = None
    position = 0
    for number, line in enumerate(text.split("\n")):
        line_start = position
        position += len(line) + 1
        if heredoc:
            if (line.lstrip("\t") if heredoc[0] else line) == heredoc[1]:
                heredoc = None
            continue
        index = 0
        while index < len(line):
            character = line[index]
            if quote == "'":
                if character == "'":
                    quote = None
            elif quote == '"':
                if character == "\\":
                    index += 1
                elif character == '"':
                    quote = None
            elif character == "\\":
                index += 1
            elif character in "'\"":
                quote = character
            elif character == "<" and SHELL_HEREDOC.match(line, index):
                match = SHELL_HEREDOC.match(line, index)
                pending_heredoc = (match.group(1) == "-", match.group(3))
                index = match.end() - 1
            elif character == "#" and (index == 0 or line[index - 1] in SHELL_COMMENT_LEADERS):
                if not (allow_shebang and number == 0 and line.startswith("#!")):
                    comments.append((base + line_start + index, base + line_start + len(line)))
                break
            index += 1
        if pending_heredoc:
            heredoc = pending_heredoc
            pending_heredoc = None
    return comments


def simple_findings(path, text, spans):
    """Turn comment offsets into findings, leaving out license notices.

    @param path The file's path.
    @param text The file's text.
    @param spans The comments as (start, end) offsets.
    @return The findings.
    """
    starts = line_starts(text)
    findings = []
    for start, end in spans:
        comment = text[start:end]
        if not is_license(comment):
            findings.append(Finding(path, line_of(starts, start), start, end, "comment", comment))
    return findings


def scan_shell(path, text):
    """Find the comments in a shell script.

    @param path The file's path.
    @param text The script.
    @return The findings.
    """
    return simple_findings(path, text, shell_comments(text))


def scan_cmake(path, text):
    """Find the comments in a CMake file, line and bracket comments both.

    @param path The file's path.
    @param text The file.
    @return The findings.
    """
    spans = []
    index = 0
    while index < len(text):
        character = text[index]
        bracket = re.match(r"\[(=*)\[", text[index:index + 20])
        if character == "#":
            bracket_comment = re.match(r"#\[(=*)\[", text[index:index + 20])
            if bracket_comment:
                close = text.find("]" + bracket_comment.group(1) + "]", index)
                end = len(text) if close < 0 else close + len(bracket_comment.group(1)) + 2
            else:
                newline = text.find("\n", index)
                end = len(text) if newline < 0 else newline
            spans.append((index, end))
            index = end
        elif character == '"':
            index += 1
            while index < len(text) and text[index] != '"':
                index += 2 if text[index] == "\\" else 1
            index += 1
        elif bracket:
            close = text.find("]" + bracket.group(1) + "]", index)
            index = len(text) if close < 0 else close + len(bracket.group(1)) + 2
        else:
            index += 1
    return simple_findings(path, text, spans)


def yaml_line_comment(line):
    """Find where a YAML line's comment starts, outside quotes.

    @param line One line of YAML.
    @return The column of the '#', or -1 for none.
    """
    quote = None
    for index, character in enumerate(line):
        if quote:
            if character == quote:
                quote = None
        elif character in "'\"" and (index == 0 or line[index - 1] in " :[{,-"):
            quote = character
        elif character == "#" and (index == 0 or line[index - 1] in " \t"):
            return index
    return -1


def scan_yaml(path, text):
    """Find the comments in a YAML file, reading each run: block as shell.

    @param path The file's path.
    @param text The file.
    @return The findings.
    """
    spans = []
    lines = text.split("\n")
    starts = line_starts(text)
    number = 0
    while number < len(lines):
        line = lines[number]
        column = yaml_line_comment(line)
        if column >= 0:
            spans.append((starts[number] + column, starts[number] + len(line)))
        block = YAML_BLOCK_SCALAR.match(line)
        number += 1
        if not block:
            continue
        key_column = len(block.group(1))
        first = number
        while number < len(lines) and (not lines[number].strip() or len(lines[number]) - len(lines[number].lstrip()) > key_column):
            number += 1
        if block.group(2) == "run" and first < number:
            content = "\n".join(lines[first:number])
            spans.extend(shell_comments(content, starts[first], allow_shebang=False))
    return simple_findings(path, text, spans)


def scan_xml(path, text):
    """Find the <!-- --> comments in an XML file.

    @param path The file's path.
    @param text The file.
    @return The findings.
    """
    spans = []
    for match in re.finditer(r"<!--.*?-->", text, re.S):
        spans.append((match.start(), match.end()))
    return simple_findings(path, text, spans)


def scan_hash_lines(path, text):
    """Find the comments in a file whose comments are lines starting with '#'.

    @param path The file's path.
    @param text The file.
    @return The findings.
    """
    spans = []
    for match in re.finditer(r"(?m)^[ \t]*#.*$", text):
        spans.append((match.start() + len(match.group(0)) - len(match.group(0).lstrip()), match.end()))
    return simple_findings(path, text, spans)
