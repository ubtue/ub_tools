CREATE TABLE full_text_cache (
       id VARCHAR(20) NOT NULL,
       url VARCHAR(255) NOT NULL,
       expiration DATETIME NOT NULL,
       status ENUM('stored', 'error'),
       error_message MEDIUMTEXT,
       PRIMARY KEY (id)
);

