import pytest
from pytest_bdd import given, parsers, scenario, scenarios, then, when


@scenario("bdd.feature", "Publishing the message")
def test_publish(user_info):
    print(f"publish: state: {user_info}")
    assert True


@pytest.fixture
def user_info():
    return {}


@given(parsers.cfparse("I'm an author {author:w}"), target_fixture="author_name")
def setup_author(author):
    return author


@when(parsers.cfparse("I create message {msg:w}"))
def create_message(author_name, user_info, msg):
    if author_name in user_info:
        user_info[author_name].append(msg)
    else:
        user_info[author_name] = [msg]


@then(parsers.cfparse("I should have the {msg:w} message"))
def check_message(author_name, user_info, msg):
    assert msg in user_info[author_name]


scenarios("bdd.feature")


@given(parsers.parse("number {one:d}"), target_fixture="number_one")
def given_number_one(one):
    return one


@when(parsers.cfparse("i add number {two:d}"), target_fixture="number_two")
def when_add_number(two):
    return two


@then(parsers.cfparse("i should got number {sum:d}"))
def then_sum_numbers(number_one, number_two, sum):
    print(f"sum numbers: {number_one}+{number_two} == {sum}")
    assert number_one + number_two == sum
