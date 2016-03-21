CREATE TABLE translations (
    id INT(11) NOT NULL,
    language_code CHAR(3) NOT NULL,
    text VARCHAR(100) NOT NULL,
    token VARCHAR(200), -- only used and then required for 'vufind_translations'
    category ENUM('keywords','vufind_translations') NOT NULL,
    preexists BOOLEAN NOT NULL DEFAULT FALSE,
    CONSTRAINT UNIQUE id_language_code_and_category (id,language_code,category),
    CONSTRAINT token_category_contraint CHECK (
        (token IS NOT NULL AND category = 'vufind_translations')
        OR (token IS NULL AND category = 'keywords')
    )
);

CREATE INDEX translations_idx_id ON translations (id);
CREATE INDEX translations_idx_language_code ON translations (language_code);
CREATE INDEX translations_idx_token ON translations (token);
CREATE INDEX translations_idx_text ON translations (text);
CREATE INDEX category_idx_text ON translations (category);
