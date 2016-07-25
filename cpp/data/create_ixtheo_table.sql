CREATE TABLE ixtheo_notations (
       id INT(11) NOT NULL,
       ixtheo_notation_list VARCHAR(8192) NOT NULL,
       FOREIGN KEY (id) REFERENCES user(id)
);

CREATE TABLE ixtheo_id_result_sets (
       id INT(11) NOT NULL,
       ids VARCHAR(128) NOT NULL,
       FOREIGN KEY (id) REFERENCES search(id)
);

CREATE TABLE ixtheo_journal_subscriptions (
       id INT(11) NOT NULL,
       journal_title VARCHAR(256) NOT NULL,
       journal_author VARCHAR(256) NOT NULL,
       journal_year VARCHAR(32) NOT NULL,
       journal_control_number VARCHAR(256) NOT NULL,
       last_issue_date DATE NOT NULL,
       FOREIGN KEY (id) REFERENCES user(id),
       PRIMARY KEY (id,journal_control_number)
);

CREATE TABLE ixtheo_user (
       id INT(11) NOT NULL,
       title VARCHAR(64),
       institution VARCHAR(256),
       country VARCHAR(256),
       FOREIGN KEY (id) REFERENCES user(id),
       PRIMARY KEY (id)
);
