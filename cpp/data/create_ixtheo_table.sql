CREATE TABLE ixtheo_notations (
       id INT(11) NOT NULL,
       ixtheo_notation_list VARCHAR(8192) NOT NULL,
       FOREIGN KEY (id) REFERENCES user(id) ON DELETE CASCADE
);

CREATE TABLE ixtheo_id_result_sets (
       id INT(11) NOT NULL,
       ids VARCHAR(128) NOT NULL,
       FOREIGN KEY (id) REFERENCES search(id) ON DELETE CASCADE
);

CREATE TABLE ixtheo_journal_subscriptions (
       id INT(11) NOT NULL,
       journal_control_number VARCHAR(256) NOT NULL,
       last_issue_date CHAR(6) NOT NULL,
       FOREIGN KEY (id) REFERENCES user(id) ON DELETE CASCADE
);
