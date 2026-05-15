"""
PlatformIO pre-build script: inject `#include "json_helper.h"` near the top
of ESPHome's generated main.cpp so global declarations emitted by ESPHome
(line ~130) can resolve types defined in json_helper.h (e.g. planthub::RuleRow
used by the global vector in rule_engine.yaml).

ESPHome emits user `esphome.includes:` entries near line ~340, well after the
globals block — so without an earlier include the compile fails with
"'planthub' was not declared in this scope".

Why patch main.cpp instead of using a `-include` build flag:
  - `build_flags = -include json_helper.h` lands in CCFLAGS, so it's force-fed
    into ESP-IDF C sources too (e.g. lwip_fast_select.c). The header pulls in
    <cmath>/<string> — C++ only — and the C compile dies.
  - Adding to CXXFLAGS scopes it to C++, but then every C++ compile (including
    ESP-IDF's cxx component, which is built outside the project's src/) needs
    the header on its include path. ESP-IDF components don't have src/ on the
    search path, so the force-include can't find the file.
  - The only file that actually needs the early include is main.cpp. Patching
    it directly is the narrowest fix and avoids fighting build flags.

Re-runs cleanly: skips the patch if the include is already present.
"""

import os

Import("env")  # noqa: F821 — PlatformIO built-in

INCLUDE_LINE = '#include "json_helper.h"\n'
SENTINEL = '#include "json_helper.h"'
ANCHOR = '#include "esphome.h"'


def patch_main_cpp():
    src_dir = env.subst("$PROJECT_SRC_DIR")
    main_cpp = os.path.join(src_dir, "main.cpp")

    if not os.path.isfile(main_cpp):
        # main.cpp doesn't exist yet on first run before ESPHome codegen.
        # PlatformIO calls extra_scripts after ESPHome's codegen, so this
        # branch only fires if something is badly out of order.
        print(f"[force-include] WARNING: {main_cpp} not found, skipping")
        return

    with open(main_cpp, "r", encoding="utf-8") as f:
        content = f.read()

    anchor_pos = content.find(ANCHOR)
    if anchor_pos == -1:
        print(f"[force-include] WARNING: anchor '{ANCHOR}' not found in main.cpp")
        return

    # ESPHome's `esphome.includes:` block emits a `#include "json_helper.h"`
    # line further down in main.cpp (line ~340). Detecting *any* occurrence
    # would falsely treat that as our injection — we only consider the patch
    # applied if the include appears before the anchor.
    if SENTINEL in content[:anchor_pos]:
        return

    patched = content[:anchor_pos] + INCLUDE_LINE + content[anchor_pos:]

    with open(main_cpp, "w", encoding="utf-8") as f:
        f.write(patched)

    print(f"[force-include] Injected json_helper.h include into {main_cpp}")


patch_main_cpp()
