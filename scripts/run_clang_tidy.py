#!/usr/bin/env python3
"""Run clang-tidy over this repo's own components (components/, esphome/components/).

Builds compiler flags from compile_commands.json, using esphome.espidf.idedata
(ships with the pinned esphome package). Vanilla LLVM clang-tidy has no Xtensa
backend, so for Xtensa targets we compile in a generic 32-bit mode with the same
flag/macro substitutions ESPHome's own script/clang-tidy uses upstream
(github.com/esphome/esphome script/clang-tidy): drop GCC-only flags, pretend to
be Xtensa via defines, and stub pgmspace.h.

compile_commands.json normally only exists after a full firmware build (ninja
compiles every source file, ~10-20 minutes). We don't need that: `esphome compile
--only-generate` (codegen only) followed by ESP-IDF's cmake *configure* step
(toolchain.run_reconfigure(), no ninja) produces an accurate compile_commands.json
in under a minute. This script does that itself by default -- no manual build step
needed first. Pass --skip-regen to reuse whatever's already on disk (e.g. from a
real `esphome compile`, or to avoid re-running it across repeated local iterations).
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(
    0, str(Path(__file__).resolve().parent.parent / ".venv" / "lib" / f"python{sys.version_info.major}.{sys.version_info.minor}" / "site-packages")
)

REPO_ROOT = Path(__file__).resolve().parent.parent
COMPONENT_DIRS = ("components", "esphome/components")

# GCC-only flags clang doesn't understand, and ones that would recreate real
# Xtensa codegen we're not attempting. Verbatim from esphome/esphome's
# script/clang-tidy.
OMIT_FLAGS = {
    "-free",
    "-fipa-pta",
    "-fstrict-volatile-bitfields",
    "-mlongcalls",
    "-mtext-section-literals",
    "-mdisable-hardware-atomics",
    "-mfix-esp32-psram-cache-issue",
    "-mfix-esp32-psram-cache-strategy=memw",
    "-fno-tree-switch-conversion",
    "-freorder-blocks",
    "-fno-jump-tables",
    "-fno-shrink-wrap",
    "-mno-target-align",
}


def git_files(patterns: list[str]) -> list[str]:
    out = subprocess.run(
        ["git", "ls-files", *patterns],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    return [line for line in out.stdout.splitlines() if line]


def clang_options(idedata: dict, build_dir: Path) -> list[str]:
    cmd: list[str] = []
    triplet = Path(idedata["cxx_path"]).name[:-4]  # strip "-g++"

    if triplet.startswith("xtensa-"):
        # clang has an Xtensa frontend but no real backend for it here (no
        # esp-clang installed); compile in generic 32-bit mode and pretend to
        # be Xtensa via defines, same trick ESPHome's own script/clang-tidy uses.
        # Force a Linux/ELF target explicitly (not the host default) so this
        # works the same on a macOS dev machine as on upstream's Linux CI --
        # ESP-IDF's IRAM_ATTR section attribute and GNU attribute syntax
        # otherwise don't parse under Mach-O/Darwin's clang defaults.
        cmd += [
            "--target=i686-pc-linux-gnu",
            "-m32",
            "-U__i386__",
            "-U__x86_64__",
            "-D__XTENSA__",
            "-D__XTENSA_EL__",
            "-D_LIBC",
        ]
    else:
        cmd += [f"--target={triplet}", "-Qunused-arguments"]
        cmd += ["-nostdinc++"]

    cmd += [
        "-nostdinc",
        "-D_PGMSPACE_H_",
        "-Dpgm_read_byte(s)=(*(const uint8_t *)(s))",
        "-Dpgm_read_byte_near(s)=(*(const uint8_t *)(s))",
        "-Dpgm_read_word(s)=(*(const uint16_t *)(s))",
        "-Dpgm_read_dword(s)=(*(const uint32_t *)(s))",
        "-Dpgm_read_ptr(s)=(*(const void *const *)(s))",
        "-DPROGMEM=",
        "-DPGM_P=const char *",
        "-DPSTR(s)=(s)",
        "-DPSTRN(s, n)=(s)",
        "-Ddeprecated(x)=",
        "-DCLANG_TIDY",
        "-D_GLIBCXX_HAVE_TLS",
    ]

    def strip_esp_march(flag: str) -> str:
        if flag.startswith("-march=") and triplet.startswith("riscv"):
            return re.sub(r"_xesp\w+", "", flag)
        return flag

    cmd += [
        strip_esp_march(flag)
        for flag in idedata["cxx_flags"]
        if flag not in OMIT_FLAGS
        and not flag.startswith("-Werror")
        and not flag.startswith("-std=")
        and not flag.startswith("-mtune=esp")
    ]
    cmd.append("-std=gnu++20")

    cmd += [f"-D{define}" for define in idedata["defines"]]

    toolchain_dir = os.path.normpath(f"{idedata['cxx_path']}/../../")
    for directory in idedata["includes"]["toolchain"]:
        if directory.startswith(toolchain_dir) and "picolibc" not in directory:
            cmd += ["-isystem", directory]

    # Our own component sources live under <build_dir>/src/...; keep those as
    # -I (diagnosed) and everything else (esp-idf framework, managed
    # components, ...) as -isystem (included but not diagnosed).
    project_src = str(build_dir / "src")
    project_includes: list[str] = []
    for directory in idedata["includes"]["build"]:
        if directory.startswith(project_src) and "managed_components" not in directory:
            project_includes.append(directory)
        else:
            cmd += ["-isystem", directory]
    cmd += [f"-I{d}" for d in project_includes]
    cmd += ["-I", project_src]

    return cmd


def _patch_pio_wrapped_mirror_urls() -> None:
    """Work around an esphome bug hit on a cold cache (e.g. fresh CI runner):
    something in the native-ESP-IDF framework download path can hand
    ``download_from_mirrors`` a PlatformIO-style ``owner/pkg@url`` spec
    instead of a plain URL, which ``requests.get()`` then rejects with
    InvalidSchema. Once a version's framework is extracted once, this
    codepath isn't hit again, so it only bites the very first run.

    Wrap ``download_from_mirrors`` to strip any such prefix from each mirror
    before it's used, regardless of which caller produced it. Patched on the
    ``esphome.espidf.framework`` module specifically -- it imports the name
    with ``from esphome.framework_helpers import download_from_mirrors``, so
    patching the original module's attribute wouldn't affect that call site.
    """
    import esphome.espidf.framework as espidf_framework

    original = espidf_framework.download_from_mirrors

    def patched(mirrors, substitutions, target, timeout=30):
        cleaned = [m.split("@", 1)[1] if "@http" in m else m for m in mirrors]
        return original(cleaned, substitutions, target, timeout=timeout)

    espidf_framework.download_from_mirrors = patched


def regenerate_compile_commands(device: str) -> None:
    """Codegen + cmake-configure (no ninja build) to (re)produce compile_commands.json."""
    _patch_pio_wrapped_mirror_urls()

    import esphome.__main__ as esphome_main

    sys.argv = ["esphome", "-q", "compile", "--only-generate", f"config/{device}.yaml"]
    os.chdir(REPO_ROOT)
    rc = esphome_main.main()
    if rc:
        raise SystemExit(f"error: 'esphome compile --only-generate' failed (exit {rc})")

    from esphome.espidf import toolchain

    rc = toolchain.run_reconfigure()
    if rc:
        raise SystemExit(f"error: ESP-IDF cmake reconfigure failed (exit {rc})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="satellite1", help="config/<device>.yaml build to use")
    parser.add_argument("--fix", action="store_true", help="apply clang-tidy fix-its")
    parser.add_argument(
        "--skip-regen",
        action="store_true",
        help="reuse the compile_commands.json already on disk instead of regenerating it",
    )
    parser.add_argument("files", nargs="*", help="repo-relative files to check (default: all component sources)")
    args = parser.parse_args()

    build_dir = REPO_ROOT / "config" / ".esphome" / "build" / args.device
    compile_commands = build_dir / "build" / "compile_commands.json"

    if not args.skip_regen:
        regenerate_compile_commands(args.device)

    if not compile_commands.is_file():
        print(f"error: no compile database at {compile_commands}", file=sys.stderr)
        return 1

    from esphome.espidf.idedata import idedata_from_build

    idedata = idedata_from_build(compile_commands)
    options = clang_options(idedata, build_dir)

    files = args.files or git_files([f"{d}/*.cpp" for d in COMPONENT_DIRS] + [f"{d}/*.cc" for d in COMPONENT_DIRS])

    project_src = build_dir / "src"
    status = 0
    for f in files:
        rel = f
        for d in COMPONENT_DIRS:
            if rel.startswith(d + "/"):
                rel = rel[len(d) + 1 :]
                break
        build_src = project_src / "esphome" / "components" / rel
        if not build_src.is_file():
            print(f"warning: {f} has no matching build artifact ({build_src}); rebuild to include it", file=sys.stderr)
            continue

        invocation = ["clang-tidy"]
        if args.fix:
            invocation.append("--fix")
        invocation.append(f"--header-filter={re.escape(str(project_src))}/.*")
        invocation.append(str(build_src))
        invocation.append("--")
        invocation.extend(options)

        print(f"==> {f}")
        result = subprocess.run(invocation)
        if result.returncode != 0:
            status = 1

    return status


if __name__ == "__main__":
    sys.exit(main())
