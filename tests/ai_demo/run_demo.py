#!/usr/bin/env python3
"""Non-destructive storyboard harness for the ERDFlow AI video experiment.

No API requests, GUI automation, project changes, or recording are performed.
"""
import argparse
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
CONFIG = HERE / "demo_config.json"
PROMPT = HERE / "database_prompt.md"
PHASES = ("intro_seconds", "conceptual_seconds", "conversion_seconds", "final_seconds")

def check():
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    if not PROMPT.is_file() or not PROMPT.read_text(encoding="utf-8").strip():
        raise ValueError("Missing or empty database prompt")
    if config.get("mode") != "replay":
        raise ValueError("Initial scaffold supports only storyboard/replay planning")
    if config.get("ai", {}).get("enabled"):
        raise ValueError("Live AI integration is not implemented")
    steps = config["animation"]
    for phase in PHASES:
        value = steps[phase]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or value <= 0:
            raise ValueError(f"{phase} must be a positive duration")
    return config

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    options = parser.add_mutually_exclusive_group(required=True)
    options.add_argument("--check", action="store_true", help="validate the experiment configuration")
    options.add_argument("--storyboard", action="store_true", help="print deterministic recording timeline")
    args = parser.parse_args()
    config = check()
    if args.check:
        print("OK: isolated demo configuration and prompt validated.")
        return
    start = 0.0
    labels = ("Prompt", "Conceptual Design", "Conceptual to Schema", "Final relational view")
    for key, label in zip(PHASES, labels):
        finish = start + config["animation"][key]
        print(f"{start:5.1f}–{finish:5.1f}s  {label}")
        start = finish
    print("\nStoryboard only: AI execution, native replay, conversion, and recording are not yet wired.")

if __name__ == "__main__":
    main()
