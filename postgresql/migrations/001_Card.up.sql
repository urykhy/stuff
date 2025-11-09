CREATE TABLE card (
    id    SERIAL4 NOT NULL,
    name  VARCHAR,
    CONSTRAINT card_pkey PRIMARY KEY (id)
);

INSERT INTO card (name) VALUES ('name-1');
INSERT INTO card (name) VALUES ('foo-2');
INSERT INTO card (name) VALUES ('bar-3');
