"""Arduino sketch declarations from preprocessed, active sketch functions."""
import re
import subprocess


def parameter_end(text, begin):
    depth = 0
    quote = None
    escaped = False
    for index in range(begin, len(text)):
        ch = text[index]
        if quote:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
        elif ch in "\"'":
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("Unterminated function parameter list")


def without_defaults(signature):
    """Remove top-level argument defaults, retaining nested declarators."""
    result = []
    depth = 0
    skip = False
    quote = None
    escaped = False
    for ch in signature:
        if quote:
            if not skip:
                result.append(ch)
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            continue
        if ch in "\"'":
            quote = ch
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                skip = False
        elif ch == "=" and depth == 1:
            skip = True
        elif ch == "," and depth == 1:
            skip = False
        if not skip:
            result.append(ch)
    return "".join(result)


def add_prototypes(preprocessed, ctags):
    lines = preprocessed.read_text().splitlines(keepends=True)
    origins = []
    source_lines = []
    origin = ""
    source_line = 0
    for line in lines:
        marker = re.match(r'# (\d+) "([^"]+)"', line)
        if marker:
            source_line = int(marker[1]) - 1
            origin = marker[2]
        origins.append(origin)
        source_lines.append(source_line)
        source_line += 1
    tag_source = preprocessed.with_name("sketch-functions.cpp")
    tag_source.write_text("".join(
        line if origin.endswith(".ino") and not line.startswith("#") else "\n"
        for line, origin in zip(lines, origins)))
    tags = subprocess.check_output([
        str(ctags), "-u", "--language-force=c++", "-f", "-",
        "--c++-kinds=f", "--fields=KSTtzns", str(tag_source)], text=True)
    prototypes = []
    for tag in tags.splitlines():
        parts = tag.split("\t")
        fields = dict(p.split(":", 1) for p in parts[3:] if ":" in p)
        if not all(k in fields for k in ("line", "returntype", "signature")):
            continue
        index = int(fields["line"]) - 1
        if not origins[index].endswith(".ino") or "class" in fields:
            continue
        signature = fields["signature"]
        storage = "static " if re.match(r"\s*static\b", lines[index]) else ""
        prototypes.append(storage + fields["returntype"] + " " + parts[0] + signature + ";\n")
        if "=" in signature:
            # ctags normalizes whitespace. Match the actual declaration to avoid
            # changing calls or body expressions with the same function name.
            match = re.search(r"\b" + re.escape(parts[0]) + r"\s*(\()", lines[index])
            if not match:
                raise ValueError("Cannot locate definition for " + parts[0])
            joined = "".join(lines[index:])
            begin = match.start(1)
            end = parameter_end(joined, begin)
            original = joined[begin:end + 1]
            count = original.count("\n")
            prefix = "".join(lines[index:index + count + 1])
            lines[index:index + count + 1] = [
                prefix[:begin] + without_defaults(original) + prefix[end + 1:]
            ] + [""] * count
    if not prototypes:
        raise ValueError("ctags produced no sketch function declarations")
    start = next(i for i, line in enumerate(lines) if line.startswith("void setup(void)"))
    lines[start:start] = ['# 1 "generated-sketch-prototypes"\n', *prototypes,
                         f'# {source_lines[start]} "{origins[start]}"\n']
    preprocessed.write_text("".join(lines))
