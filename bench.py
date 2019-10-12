#! /usr/bin/env python3

import glob
import os
import shutil
import subprocess
import sys


CLANG_DB_NAME = "compile_commands.json"


def wrapCommand(cmd):
    """Run the given command, as a list.

    Pipes the subprocess's stdout and stderr to ours. Exits with a message on
    failure.
    """
    ret = subprocess.Popen(cmd, stdout=sys.stdout, stderr=sys.stderr).wait()
    if ret != 0:
        sys.stderr.write(f"{cmd[0]} failed. Aborting.\n")
        exit(ret)


def main():
    if os.path.exists("build"):
        shutil.rmtree("build")
    os.mkdir("build")
    os.chdir("build")
    # -DCMAKE_EXPORT... tells cmake to generate a clang compilation database
    # for tooling.
    wrapCommand(["cmake", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                          "-DCMAKE_BUILD_TYPE=Release", ".."])
    # Put the clang DB at the project directory so tools can find it.
    os.rename(CLANG_DB_NAME, f"../{CLANG_DB_NAME}")

    wrapCommand(["make"])

    benches = sorted(glob.iglob("../bench/*cpp"))

    print(benches)

    for bench in benches:
        stripped = os.path.basename(bench)[:-4]
        wrapCommand([f"./{stripped}"])
        print("\n")


if __name__ == "__main__":
    main()
