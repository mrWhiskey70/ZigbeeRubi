"""Host persistence is owned by the C++ FileStore, not the Python HTTP bridge.

Files: slot0/1.bin, commit0/1.bin, devices0.bin. Each replacement is flushed
and renamed; scenario generations are published by ScenarioStore commit records.
"""
