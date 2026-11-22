"""Formats a C++ file in this project with clang-format after Claude Code edits it.

Runs as a PostToolUse hook on Edit and Write. Reads the hook input from stdin.
"""

import json
import os
import subprocess
import sys

path = json.load(sys.stdin).get("tool_input", {}).get("file_path", "")
project = os.path.normcase(os.path.abspath(os.environ.get("CLAUDE_PROJECT_DIR", os.getcwd())))
in_project = os.path.normcase(os.path.abspath(path)).startswith(project + os.sep)
if in_project and path.lower().endswith((".cpp", ".hpp")):
    subprocess.run(["clang-format", "-i", path], check=False)
