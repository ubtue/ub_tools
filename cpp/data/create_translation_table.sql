CREATE TABLE translations (
       id INT(11) NOT NULL,
       language_code CHAR(3) NOT NULL,
       text VARCHAR(100) NOT NULL,
       CONSTRAINT UNIQUE id_and_language_code (id,language_code)
);

CREATE INDEX translations_index ON translations (id);
