CREATE TABLE relbib_ids (
  record_id VARCHAR(10) NOT NULL PRIMARY KEY) DEFAULT CHARSET=utf8mb4;

GRANT DROP ON vufind.relbib_ids TO 'vufind'@'localhost';

CREATE VIEW resource_tags_relbib AS (
  SELECT * FROM resource_tags WHERE resource_id IN 
  (SELECT resource.id FROM resource JOIN relbib_ids 
   ON resource.record_id = relbib_ids.record_id)
);

CREATE TABLE bibstudies_ids (
  record_id VARCHAR(10) NOT NULL PRIMARY KEY) DEFAULT CHARSET=utf8mb4;

GRANT DROP ON vufind.bibstudies_ids TO 'vufind'@'localhost';

CREATE VIEW resource_tags_bibstudies AS (
  SELECT * FROM resource_tags WHERE resource_id IN 
  (SELECT resource.id FROM resource JOIN bibstudies_ids
   ON resource.record_id = bibstudies_ids.record_id)
);

