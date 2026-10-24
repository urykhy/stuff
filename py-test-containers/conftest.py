import psycopg
import pytest
from testcontainers.community.postgres import PostgresContainer


@pytest.fixture(scope="session")
def postgres():
    with PostgresContainer("postgres:17.4-alpine", driver=None) as pg:
        yield pg


@pytest.fixture(scope="session")
def sql(postgres):
    def x(query):
        url = postgres.get_connection_url()
        with psycopg.connect(url) as connection, connection.cursor() as cursor:
            return cursor.execute(query).fetchall()

    return x
