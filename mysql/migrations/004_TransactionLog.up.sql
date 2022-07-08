CREATE TABLE transaction_log (
    service VARCHAR(32) NOT NULL,
    task    VARCHAR(32) NOT NULL,
    id      VARCHAR(32) NOT NULL,
    created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (service, task, id),
    KEY (created),
    KEY (task)
) ENGINE = INNODB DEFAULT CHARSET = utf8;