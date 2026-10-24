def test_check(check, sql):
    (version,) = sql("SELECT version()")[0]
    check.is_true(version.startswith("PostgreSQL 17.4"))
