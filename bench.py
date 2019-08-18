#! /usr/bin/env python3

import os
import subprocess
import sys

CLANG_DB_NAME="compile_commands.json"

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
    if not os.path.exists("build"):
        os.mkdir("build")
    os.chdir("build")
    # -DCMAKE_EXPORT... tells cmake to generate a clang compilation database for
    # tooling.
    wrapCommand(["cmake", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                          "-DCMAKE_BUILD_TYPE=Release", ".."])
    # Put the clang DB at the project directory so tools can find it.
    os.rename(CLANG_DB_NAME, f"../{CLANG_DB_NAME}")

    wrapCommand(["make"])
    wrapCommand(["./bench"])

if __name__ == "__main__":
    main()
