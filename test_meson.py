#!/usr/bin/env python3
#
# magic to run tests via vscode native testing

import dis
import glob
import logging
import os
import shelve
import subprocess
from xml.etree.ElementTree import fromstring

import pytest

PROJECTS = ["prometheus", "protobuf", "mysql", "cache"]
LOGGER = logging.getLogger(__name__)


def shelve_cache(f):
    def inner(*args):
        name = args[0].replace("/", "_")
        key = f.__name__ + ";" + ";".join(args)
        with shelve.open(f"__pycache__/shelve_{name}") as db:
            if key in db:
                return db[key]
            res = f(*args)
            db[key] = res
            return res

    return inner


@shelve_cache
def meson_test_names(dirname):
    names = []
    p = run_with_args(dirname, True, ("test", "--no-rebuild", "--list"))
    for name in p.stdout.decode().splitlines():
        names.append(name)
    return names


@shelve_cache
def boost_test_names(dirname, testname):
    names = []
    current = []
    run_with_args(
        dirname,
        True,
        ("test", testname, "--no-rebuild", "--test-args", "\\--list_content"),
    )
    logname = os.path.join(dirname, "meson-logs/testlog.txt")
    with open(logname) as f:
        for line in f:
            if "----------------------------------- stderr" in line:
                line = ""
                break
        for line in f:
            if "===================================" in line:
                break
            c = int((len(line) - len(line.lstrip())) / 4)
            current = current[0:c]
            current.append(line.strip().strip("*"))
            names.append("/".join(current))
    names = list(filter(lambda x: not any(y.startswith(x + "/") for y in names), names))
    return names


def prepare_cases():
    cases = {}
    for d in PROJECTS:
        for x in glob.glob(f"{d}/build*/build.ninja"):
            dirname = os.path.dirname(x)
            if dirname not in cases:
                cases[dirname] = {}
            for name in meson_test_names(dirname):
                if name not in cases[dirname]:
                    cases[dirname][name] = []
                if name.startswith("fuzz"):
                    cases[dirname][name].append(("fuzz", name))
                else:
                    for bt in boost_test_names(dirname, name):
                        cases[dirname][name].append((name, bt))
    return cases


def load_xml(filename):
    rows = ""
    with open(filename) as f:
        for line in f:
            if "----------------------------------- stdout" in line:
                line = ""
                break
        for line in f:
            if (
                "----------------------------------- stderr" in line
                or "===================================" in line
            ):
                break
            rows += line
    return fromstring(rows)


def assert_xml(dirname, doc):
    __tracebackhide__ = True

    def step(dirname, doc, name):
        __tracebackhide__ = True

        def f():
            pass

        for x in doc.findall(f".//{name}"):
            parent = doc.find(f".//{name}[@line=\"{x.attrib['line']}\"]..")
            PY_CODE_LOCATION_INFO_NO_COLUMNS = 13
            f.__code__ = f.__code__.replace(
                co_firstlineno=int(x.attrib["line"]),
                co_filename=os.path.abspath(os.path.join(dirname, x.attrib["file"])),
                co_name=parent.attrib["name"],
                co_code=bytes(
                    [
                        dis.opmap["RESUME"],
                        0,
                        dis.opmap["LOAD_ASSERTION_ERROR"],
                        0,
                        dis.opmap["RAISE_VARARGS"],
                        1,
                    ]
                ),
                co_linetable=bytes(
                    [
                        (1 << 7) | (PY_CODE_LOCATION_INFO_NO_COLUMNS << 3) | (3 - 1),
                        0,
                    ]
                ),
            )
            f()

    step(dirname, doc, "Error")
    step(dirname, doc, "FatalError")
    step(dirname, doc, "Exception")


def run_with_args(dirname, check, cmd):
    LOGGER.info(f"[{dirname}]: run {cmd}")
    env = os.environ.copy()
    env["DIRENV_LOG_FORMAT"] = ""
    p = subprocess.run(
        ["/usr/bin/direnv", "exec", ".", "meson", *cmd],
        cwd=dirname,
        shell=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=check,
        env=env,
    )
    return p


def ids(tc):
    return tc[1].replace(" ", "_")


# generate classes
for dirname, cases in prepare_cases().items():
    name = "Test" + dirname.replace("/", " ").replace("-", " ").title().replace(
        " ", ""
    ).replace("Build", "")
    if not name.endswith("Release"):
        name += "Debug"

    t = type(
        name,
        (object,),
        {"dirname": dirname},
    )
    globals()[name] = t

    # generate methods
    for testname, bt in cases.items():
        methodname = testname.replace(" ", "_")

        def impl(self, tc):
            __tracebackhide__ = True
            (testname, bt) = tc
            if testname == "fuzz":
                run_with_args(self.dirname, True, ("test", bt))
            else:
                run_with_args(
                    self.dirname,
                    False,
                    (
                        "test",
                        testname,
                        "--test-args",
                        f"\\--color_output=no -f XML -t '{bt}'",
                    ),
                )
                logname = os.path.join(self.dirname, "meson-logs/testlog.txt")
                xml = load_xml(logname)
                assert_xml(self.dirname, xml)

        f = pytest.mark.parametrize("tc", bt, ids=ids)(impl)
        if testname.startswith("fuzz"):
            f = pytest.mark.slow(f)
        setattr(t, f"test_{methodname}", f)
