CREATE TABLE task_queue (
    id      BIGINT  NOT NULL AUTO_INCREMENT,
    task    VARCHAR(255) NOT NULL,
    worker  VARCHAR(255) NOT NULL DEFAULT '',
    hint    VARCHAR(255) NOT NULL DEFAULT '',
    status  ENUM('new', 'started', 'done', 'error') NOT NULL DEFAULT 'new',
    created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE  KEY (task),
    KEY (worker),
    KEY (status),
    KEY (created),
    KEY (updated)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;