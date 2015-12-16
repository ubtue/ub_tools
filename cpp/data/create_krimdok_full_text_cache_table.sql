CREATE TABLE full_text_cache (
       url VARCHAR(512) NOT NULL,
       hash CHAR(40) NOT NULL,
       full_text MEDIUMBLOB NOT NULL,
       last_used DATETIME NOT NULL,
       PRIMARY KEY (url)
);

