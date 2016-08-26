CREATE TABLE vufind_translations (
    id INT(11) NOT NULL,
    language_code CHAR(3) NOT NULL,
    text VARCHAR(100) NOT NULL,
    token VARCHAR(200),
    CONSTRAINT UNIQUE id_and_language_code (id,language_code),
);

CREATE INDEX vufind_translations_idx_id ON vufind_translations (id);
CREATE INDEX vufind_translations_idx_language_code ON vufind_translations (language_code);
CREATE INDEX translations_idx_text ON translations (text);

CREATE TABLE keyword_translations (
    ppn CHAR(9) NOT NULL,
    language_code CHAR(3) NOT NULL,
    text VARCHAR(100) NOT NULL,
    CONSTRAINT UNIQUE ppn_and_language_code (ppn,language_code),
);

CREATE INDEX keyword_translations_idx_ppn ON keyword_translations (ppn);
CREATE INDEX keyword_translations_idx_language_code ON keyword_translations (language_code);
CREATE INDEX translations_idx_text ON translations (text);
