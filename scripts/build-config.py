#!/usr/bin/env python3
"""Update a configuration stamp only when effective build inputs change."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("stamp", type=Path)
parser.add_argument("--compiler", required=True)
parser.add_argument("--flags", required=True)
parser.add_argument("--sdk", type=Path)
args = parser.parse_args()
command = shlex.split(args.compiler)
compiler = Path(shutil.which(command[0]) or command[0]).resolve()
state = {"command": command, "compiler": str(compiler),
         "compiler_mtime": compiler.stat().st_mtime_ns,
         "version": subprocess.check_output(command + ["--version"]).decode(),
         "flags": args.flags}
if args.sdk:
    sdk = args.sdk.resolve()
    state["sdk"] = str(sdk)
    state["sdk_inputs"] = {str(p.relative_to(sdk)): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in sorted(sdk.glob("*.mk")) + sorted(sdk.glob("libRack.*"))}
text = json.dumps(state, sort_keys=True, indent=2) + "\n"
args.stamp.parent.mkdir(parents=True, exist_ok=True)
if not args.stamp.exists() or args.stamp.read_text() != text:
    # macOS ships Make 3.81, which compares whole-second timestamps.
    # Cross a timestamp boundary before invalidating already-built objects.
    if args.stamp.exists():
        time.sleep(1.01 - (time.time() % 1))
    args.stamp.write_text(text)
