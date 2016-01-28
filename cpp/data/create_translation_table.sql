CREATE TABLE translations (
       id INT(11) NOT NULL,
       language_code CHAR(3) NOT NULL,
       text VARCHAR(100) NOT NULL,
       category ENUM('keywords','vufind_translations') NOT NULL,
       preexists BOOLEAN NOT NULL DEFAULT FALSE,
       CONSTRAINT UNIQUE id_language_code_and_category (id,language_code,category)
);

CREATE INDEX translations_idx_id ON translations (id);
CREATE INDEX translations_idx_language_code ON translations (language_code);
CREATE INDEX translations_idx_text ON translations (text);
CREATE INDEX category_idx_text ON translations (category);
