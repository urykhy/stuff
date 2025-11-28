#!/usr/bin/python
# vim:ts=4:sts=4:sw=4:et

# python -m pytest sample.py --alluredir /tmp/allure --rootdir . -c pytest.ini
#

import datetime
import hashlib
import itertools
import json
import logging
import uuid

import pytest
from faker import Faker
from pydantic import BaseModel
from syrupy.filters import paths
from syrupy.matchers import path_type

import allure


def hash(s):
    return int(hashlib.sha1(s.encode("utf-8")).hexdigest(), 16) % (10**9)


class City(BaseModel):
    id: uuid.UUID
    name: str


class User(BaseModel):
    id: uuid.UUID
    name: str
    cities: list[City]
    created_at: datetime.datetime


@pytest.fixture
def logger():
    logger = logging.getLogger(__name__)
    ch = logging.StreamHandler()
    formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s")
    ch.setFormatter(formatter)
    logger.addHandler(ch)
    return logger


@pytest.fixture
def sequence():
    return itertools.count(start=1)


@pytest.fixture
def matcher(sequence):
    return path_type(types=(uuid.UUID,), replacer=lambda *_: str(next(sequence)))


@pytest.fixture
def xfaker(request):
    x = Faker()
    x.seed_instance(hash(request.node.name))
    return x


def test_snapshot1(xfaker, snapshot, matcher, logger):
    assert uuid.uuid4() == snapshot(matcher=matcher)
    s = {
        "id": xfaker.uuid4(),
        "name": xfaker.name(),
        "cities": [{"id": xfaker.uuid4(), "name": xfaker.name()}],
        "created_at": str(datetime.datetime.now()),
    }
    logger.debug(f"*** {s=}")
    user = User.model_validate_json(json.dumps(s))
    assert user.model_dump() == snapshot(matcher=matcher, exclude=paths("created_at"))


def test_snapshot2(snapshot, matcher):
    assert uuid.uuid4() == snapshot(matcher=matcher)


@pytest.mark.minor
@allure.feature("Feature1")
@allure.story("Story")
@allure.sub_suite("sub1")
@allure.severity(allure.severity_level.TRIVIAL)
# @pytest.mark.xfail(reason="this test must fail")
def test_check(check):
    check.equal(False, True)
    check.equal(True, False)


@allure.feature("Feature2")
@allure.story("Story1")
class TestBar:
    @allure.story("Story2")
    @allure.link("https://linux.org.ru", name="Link name")
    def test_bar(self, check):
        with allure.step("replace"):
            check.equal(True, True)

    @pytest.mark.parametrize("param", [True, False], ids=["param=true", "param=false"])
    @allure.description("TestBar description")
    def test_param(self, param, check):
        allure.attach(str(param), "param from test", allure.attachment_type.TEXT)
        check.equal(True, param, "fail to be sure")
