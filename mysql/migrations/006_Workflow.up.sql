CREATE TABLE workflow (
    id        BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    space     VARCHAR(255) CHARACTER SET latin1 NOT NULL,
    strand    VARCHAR(255) CHARACTER SET latin1 NULL,
    task      VARCHAR(255) CHARACTER SET latin1 NOT NULL,
    priority  SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    worker    VARCHAR(64) CHARACTER SET latin1 NOT NULL DEFAULT '',
    cookie    TEXT NULL,
    status    ENUM('new', 'started', 'done', 'error') NOT NULL DEFAULT 'new',
    created   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY   KEY (id),
    UNIQUE    KEY (space, task),
    KEY (worker(32)),
    KEY (status, updated),
    KEY (created),
    KEY (updated)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
