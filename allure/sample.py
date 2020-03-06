#!/usr/bin/python
# vim:ts=4:sts=4:sw=4:et

# python -m pytest sample.py --alluredir /tmp/allure --rootdir . -c pytest.ini
#

import pytest
import allure

@pytest.mark.minor
@allure.feature('Feature1')
@allure.story('Story')
@allure.sub_suite('sub1')
@allure.severity(allure.severity_level.TRIVIAL)
@pytest.mark.xfail(condition=lambda: True, reason='this test must fail')
def test_minor():
    assert False


@allure.feature('Feature2')
@allure.story('Story1')
class TestBar:
    @allure.story('Story2')
    @allure.link('https://linux.org.ru', name='Link name')
    def test_bar(self):
        assert True
        with allure.step("replace"):
            assert False

    @pytest.mark.parametrize('param', [True], ids=['boolean parameter id'])
    @allure.description('TestBar description')
    def test_param(self, param):
        assert param
        allure.attach(str(param), 'param from test', allure.attachment_type.TEXT)
        assert False, "fail to be sure"
