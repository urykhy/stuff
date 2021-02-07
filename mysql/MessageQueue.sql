CREATE TABLE message_state (
    service  VARCHAR(255) NOT NULL,
    position BIGINT       NOT NULL,
    PRIMARY  KEY (service)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE message_queue (
    producer VARCHAR(255) NOT NULL,
    serial   BIGINT       NOT NULL,
    task     VARCHAR(255) NOT NULL,
    hash     VARCHAR(255) NOT NULL DEFAULT '',
    created  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY  KEY (producer, serial),
    UNIQUE   KEY (producer, task)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
