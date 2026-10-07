#!/usr/bin/env python3
"""Compile and run the four unmodified upstream Tuya tests."""

import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    source = root / "audit" / "upstream"
    build = root / "build" / "audit"
    build.mkdir(parents=True, exist_ok=True)
    cases = {
        "test_tuya_dp_parser": ["tuya_dp_parser.cpp"],
        "test_tuya_contact_sensor_plugin": [
            "tuya_dp_parser.cpp", "tuya_contact_sensor_plugin.cpp"
        ],
        "test_tuya_switch_plugin": ["tuya_dp_parser.cpp", "tuya_switch_plugin.cpp"],
        "test_tuya_fingerprint": ["tuya_fingerprint.cpp"],
    }
    results = []
    for name, implementations in cases.items():
        binary = build / name
        command = [
            os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-I", str(source / "components/service/include"),
            "-I", str(source / "components/core/include"),
            str(source / "test/host" / (name + ".cpp")),
            *(str(source / "components/service" / p) for p in implementations),
            "-o", str(binary),
        ]
        try:
            compiled = subprocess.run(command, capture_output=True, text=True)
            executed = None
            if compiled.returncode == 0:
                executed = subprocess.run([str(binary)], capture_output=True, text=True)
            output = compiled.stdout + compiled.stderr
            if executed is not None:
                output += executed.stdout + executed.stderr
            result = {
                "test": name,
                "compile": compiled.returncode,
                "run": executed.returncode if executed is not None else None,
                "output": output,
            }
        except OSError as error:
            result = {"test": name, "compile": None, "run": None, "output": str(error)}
        results.append(result)
        passed = result["compile"] == 0 and result["run"] == 0
        print(f"{name}: {'PASS' if passed else 'FAIL'}")
        if result["output"]:
            print(result["output"])
    (build / "results.json").write_text(
        json.dumps(results, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return 0 if all(r["compile"] == 0 and r["run"] == 0 for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
