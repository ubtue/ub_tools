DROP TABLE IF EXISTS tuefind_publications;
CREATE TABLE tuefind_publications (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id INT NOT NULL,
    control_number VARCHAR(255) NOT NULL,
    external_document_id VARCHAR(255) NOT NULL,
    external_document_guid VARCHAR(255) DEFAULT NULL,
    terms_date DATE NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY publication_control_number (control_number),
    UNIQUE KEY publication_external_document_id (external_document_id),
    UNIQUE KEY publication_external_document_guid (external_document_guid),
    FOREIGN KEY (user_id) REFERENCES user(id) ON DELETE CASCADE
) DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_bin;
