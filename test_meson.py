#!/usr/bin/env python3
#
# magic to run tests via vscode native testing

import glob
import os
import pytest
import subprocess


def prepare_cases():
    cases = []
    for d in ["prometheus", "protobuf", "mysql"]:
        for x in glob.glob(f"{d}/build*/build.ninja"):
            x = os.path.dirname(x)
            p = subprocess.run(
                ["meson", "test", "--list"],
                cwd=x,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if len(p.stderr.decode()) == 0:
                for name in p.stdout.decode().splitlines():
                    cases.append("/".join([x, name]))
    return cases


@pytest.mark.parametrize("tc", prepare_cases())
def test_cxx(tc):
    p = subprocess.run(
        f"direnv exec . bash -c 'meson test -v \"{os.path.basename(tc)}\"'",
        cwd=os.path.dirname(tc),
        shell=True,
        check=True,
    )
