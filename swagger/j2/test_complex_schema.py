#!/usr/bin/env python3

import subprocess
from pathlib import Path

import pytest
import yaml

from .swagger import make_template

script_path = str(Path(__file__).parent.absolute())
doc = yaml.safe_load(open(script_path + "/test_complex_schema.yaml", "r"))


@pytest.fixture(scope="session")
def template():
    return make_template("/test_complex_schema.j2")


def prepare_cases():
    return doc


def prepare_ids():
    return [x["name"] for x in doc]


def clang_format(text):
    p = subprocess.Popen("clang-format", stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    p.stdin.write(text.encode("utf-8"))
    p.stdin.close()
    p.wait()
    return p.stdout.read().decode("utf-8")


@pytest.mark.parametrize("tc", prepare_cases(), ids=prepare_ids())
def test_complex_schema(tc, template, snapshot):
    r = clang_format(template.render(doc=tc).strip())
    assert r == snapshot
