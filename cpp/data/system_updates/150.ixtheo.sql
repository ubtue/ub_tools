ALTER TABLE vufind.user RENAME COLUMN ixtheo_country TO tuefind_country;
ALTER TABLE vufind.user MODIFY COLUMN tuefind_country VARCHAR(255) DEFAULT NULL AFTER last_language;
