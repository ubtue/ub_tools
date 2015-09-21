CREATE TABLE ixtheo (
       id INT(11) NOT NULL,
       ixtheo_notation_list VARCHAR(8192) NOT NULL,
       FOREIGN KEY (id) REFERENCES user(id) ON DELETE CASCADE
);

