CREATE TABLE ixtheo_notations (
       id INT(11) NOT NULL,
       ixtheo_notation_list VARCHAR(8192) NOT NULL,
       FOREIGN KEY (id) REFERENCES user(id) ON DELETE CASCADE
);

CREATE TABLE ixtheo_search_hash (
       id INT(11) NOT NULL,
       search_hash VARCHAR(128) NOT NULL,
       FOREIGN KEY (id) REFERENCES search(id) ON DELETE CASCADE
);

