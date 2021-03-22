CREATE TABLE test_data (
    id      INT          NOT NULL AUTO_INCREMENT,
    name    VARCHAR(128) NOT NULL,
    PRIMARY KEY (id)
) ENGINE = INNODB DEFAULT CHARSET = utf8;

CREATE TABLE upload_log (
    name    VARCHAR(128) NOT NULL,
    created TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (name)
) ENGINE = INNODB DEFAULT CHARSET = utf8;