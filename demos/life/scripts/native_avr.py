"""Use Homebrew's native AVR tools on Apple Silicon instead of Intel packages.

PlatformIO Atmel AVR 5.1 ships x86_64 tools even for darwin_arm64 hosts.
Other hosts retain PlatformIO's packaged tools. See README.md for dependencies.
"""

import platform
import shutil
import subprocess
from pathlib import Path

Import("env", "projenv")


def configure_native_avr():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        return

    brew = shutil.which("brew")
    if not brew:
        raise RuntimeError("Apple Silicon builds need Homebrew AVR tools; see README.md")
    prefix = Path(subprocess.check_output([brew, "--prefix"], text=True).strip())
    compiler = prefix / "opt/avr-gcc@8/bin"
    binutils = prefix / "opt/avr-binutils/bin"
    tools = {
        "CC": compiler / "avr-gcc",
        "CXX": compiler / "avr-g++",
        "AR": compiler / "avr-gcc-ar",
        "RANLIB": compiler / "avr-gcc-ranlib",
        "AS": binutils / "avr-as",
        "OBJCOPY": binutils / "avr-objcopy",
        "SIZETOOL": binutils / "avr-size",
    }
    for tool in tools.values():
        if not tool.is_file():
            raise RuntimeError(f"Missing native AVR tool: {tool}; see README.md")
    for build_env in (env, projenv):
        build_env.Replace(**{key: str(value) for key, value in tools.items()})
        build_env.PrependENVPath("PATH", [str(compiler), str(binutils)])

    # Use the native uploader and its matching config too. Preserve PlatformIO's
    # programmer, MCU, erase, port, and user flags. Never initiate an upload here.
    uploader = prefix / "opt/avrdude/bin/avrdude"
    config = prefix / "etc/avrdude.conf"
    if env.subst("$UPLOADER") == "avrdude":
        if not uploader.is_file() or not config.is_file():
            raise RuntimeError("Missing Homebrew avrdude; see README.md")
        flags = list(env["UPLOADERFLAGS"])
        flags[flags.index("-C") + 1] = str(config)
        env.Replace(UPLOADER=str(uploader), UPLOADERFLAGS=flags)
    print("Using native Homebrew AVR toolchain (Apple Silicon)")


configure_native_avr()
