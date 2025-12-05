Feature: Blog
    A site where you can publish your articles.

    Scenario: Publishing the message
        Given I'm an author foo
        When I create message bar
        When I create message baz
        Then I should have the bar message
        Then I should have the baz message

    Scenario Outline: Sum
        Given number <one>
        When i add number <two>
        Then i should got number <sum>

        Examples:
        | one | two | sum  |
        |  1  |  3  |  4   |
        |  4  |  7  |  11  |
