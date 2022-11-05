CREATE TABLE mq_producer (
    queue    VARCHAR(255)    NOT NULL,
    position BIGINT UNSIGNED NOT NULL,
    PRIMARY  KEY (queue)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE mq_consumer (
    name     VARCHAR(255)    NOT NULL,
    queue    VARCHAR(255)    NOT NULL,
    position BIGINT UNSIGNED NOT NULL,
    PRIMARY  KEY (name, queue),
    KEY (queue)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE mq_data (
    queue    VARCHAR(255)    NOT NULL,
    serial   BIGINT UNSIGNED NOT NULL,
    task     VARCHAR(255)    NOT NULL,
    created  TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY  KEY (queue, serial)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
