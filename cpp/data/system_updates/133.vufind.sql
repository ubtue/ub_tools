CREATE TABLE tuefind_user_authorities_history (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    authority_id VARCHAR(255) NOT NULL,
    user_id INT NOT NULL,
    admin_id INT NULL,
    access_state ENUM('requested', 'granted', 'declined') NOT NULL,
    request_user_date TIMESTAMP DEFAULT NOW() NOT NULL,
    process_admin_date TIMESTAMP DEFAULT NULL,
    PRIMARY KEY (id),
    FOREIGN KEY (admin_id) REFERENCES user(id) ON DELETE SET NULL,
    FOREIGN KEY (user_id) REFERENCES user(id) ON DELETE CASCADE
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
