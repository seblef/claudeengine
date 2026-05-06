#!/usr/bin/env python3
"""Expand #include directives in a GLSL shader file.

Usage: glsl_preprocess.py <shader_file> <include_base_dir>

Reads <shader_file>, recursively expands any #include <path> or
#include "path" directives relative to <include_base_dir>, and prints the
resulting source to stdout.  Each path is included at most once (pragma-once
semantics), matching the behaviour of GLShader's C++ preprocessor.
"""
import os
import re
import sys


def expand(path: str, base: str, seen: set) -> str:
    with open(path, encoding="utf-8") as f:
        lines = f.readlines()
    out = []
    for line in lines:
        m = re.match(r'\s*#include\s*[<"](.*?)[>"]\s*$', line)
        if m:
            inc = m.group(1)
            if inc not in seen:
                seen.add(inc)
                out.append(expand(os.path.join(base, inc), base, seen))
        else:
            out.append(line)
    return "".join(out)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <shader_file> <include_base_dir>",
              file=sys.stderr)
        sys.exit(1)
    print(expand(sys.argv[1], sys.argv[2], set()), end="")
