-- The default collating sequence is a Swedish one.  This leads to aliasing problems for characters with
-- diacritical marks => we need to override it and use utf8mb4_bin.

CREATE TABLE vufind_translations (
  id INT NOT NULL AUTO_INCREMENT,
  token VARCHAR(191) NOT NULL,
  language_code CHAR(4) NOT NULL,
  translation VARCHAR(1024) NOT NULL,
  translator VARCHAR(50),
  create_timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
  prev_version_id INT DEFAULT NULL,
  next_version_id INT DEFAULT NULL,
  PRIMARY KEY (id),
  KEY vufind_translations_idx_token (token),
  KEY vufind_translations_idx_language_code (language_code)
) DEFAULT CHARSET=utf8mb4;


CREATE TABLE keyword_translations (
  id INT NOT NULL AUTO_INCREMENT,
  ppn CHAR(10) NOT NULL,
  gnd_code CHAR(10) NOT NULL,
  wikidata_id CHAR(12),
  language_code CHAR(4) NOT NULL,
  translation VARCHAR(1024) NOT NULL,
  status ENUM('reliable_synonym', 'reliable', 'unreliable', 'unreliable_cat2'
              'unreliable_synonym', 'replaced', 'replaced_synonym',
              'new', 'new_synonym') NOT NULL,
  origin CHAR(3) NOT NULL,
  gnd_system VARCHAR(30),
  translator VARCHAR(50),
  german_updated TINYINT(1),
  priority_entry TINYINT(1),
  translation_disabled TINYINT(1),
  create_timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
  prev_version_id INT DEFAULT NULL,
  next_version_id INT DEFAULT NULL,
  PRIMARY KEY (id),
  KEY keyword_translations_idx_ppn (ppn),
  KEY keyword_translations_idx_language_code (language_code),
  KEY keyword_translations_idx_wikidata_id (wikidata_id),
  KEY keyword_translations_idx_translation (translation(30)),
  KEY keyword_translations_idx_gnd_code (gnd_code),
  KEY keyword_translations_idx_status (status),
  KEY keyword_translations_idx_translation_disabled (translation_disabled),
  KEY keyword_translations_idx_prev_version_id (prev_version_id),
  KEY keyword_translations_idx_next_version_id (next_version_id),
  KEY keyword_translations_idx_origin (origin)
) DEFAULT CHARSET=utf8mb4;

CREATE TABLE translators (
  translator VARCHAR(30) NOT NULL,
  translation_target VARCHAR(20) NOT NULL,
  lookfor VARCHAR(100),
  filtered_lookfor VARCHAR(100),
  offset VARCHAR(10),
  filtered_offset VARCHAR(10),
  last_notified DATETIME DEFAULT NULL,
  PRIMARY KEY (translator, translation_target)
) DEFAULT CHARSET=utf8mb4;



DELIMITER $$
DROP PROCEDURE IF EXISTS insert_vufind_translation_entry $$
CREATE PROCEDURE insert_vufind_translation_entry(IN in_token VARCHAR(191), IN in_language_code CHAR(4), IN in_translation VARCHAR(1024), IN in_translator VARCHAR(50))
SQL SECURITY INVOKER
BEGIN

DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
          GET DIAGNOSTICS CONDITION 1
          @p1 = MESSAGE_TEXT,
          @p2 = RETURNED_SQLSTATE,
          @p3 = MYSQL_ERRNO,
          @p4 = SCHEMA_NAME,
          @p5 = TABLE_NAME;
          SET @fullerror = CONCAT("ERROR: ", @p1, "|",  @p2, "|", @p3, "|",  @p4, "|", @p5);
          ROLLBACK;
          SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = @fullerror;
    END;

START TRANSACTION;

IF ((SELECT count(*) from vufind_translations WHERE token=in_token AND language_code=in_language_code AND next_version_id is null) > 1) THEN
        ROLLBACK;
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'ERROR: more than one possible entry found';
END IF;

SET @old_record_id := (SELECT id
    FROM vufind_translations
    WHERE token=in_token AND language_code=in_language_code AND next_version_id is null);

INSERT INTO vufind_translations (token,language_code,translation,translator,prev_version_id) VALUES (in_token,in_language_code,in_translation,in_translator,@old_record_id);
SET @new_record_id := LAST_INSERT_ID();

UPDATE vufind_translations SET next_version_id=@new_record_id WHERE id=@old_record_id;
COMMIT;
END $$
DELIMITER ;


DELIMITER $$
DROP PROCEDURE IF EXISTS insert_keyword_translation_entry $$
CREATE PROCEDURE insert_keyword_translation_entry(IN in_ppn CHAR(10), IN in_gnd_code CHAR(10), IN in_language_code CHAR(10), IN in_translation VARCHAR(1024), IN in_translator VARCHAR(30))
SQL SECURITY INVOKER
BEGIN

DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
          GET DIAGNOSTICS CONDITION 1
          @p1 = MESSAGE_TEXT,
          @p2 = RETURNED_SQLSTATE,
          @p3 = MYSQL_ERRNO,
          @p4 = SCHEMA_NAME,
          @p5 = TABLE_NAME;
          SET @fullerror = CONCAT("ERROR: ", @p1, "|",  @p2, "|", @p3, "|",  @p4, "|", @p5);
          ROLLBACK;
          SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = @fullerror;
    END;

START TRANSACTION;

IF ((SELECT count(*) from keyword_translations WHERE ppn=in_ppn AND gnd_code=in_gnd_code AND language_code=in_language_code AND (status='new' OR status='reliable' OR status='replaced') AND next_version_id is null) > 1) THEN
        ROLLBACK;
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'ERROR: more than one possible entry found';
END IF;

SET @old_record_id := (SELECT id
    FROM keyword_translations
    WHERE ppn=in_ppn AND gnd_code=in_gnd_code AND language_code=in_language_code AND (status='new' OR status='reliable' OR status='replaced') AND next_version_id is null);

INSERT INTO keyword_translations (ppn,gnd_code,language_code,translation,origin,status,translator,prev_version_id) VALUES (in_ppn,in_gnd_code,in_language_code,in_translation,150,'new',in_translator,@old_record_id);
SET @new_record_id := LAST_INSERT_ID();

UPDATE keyword_translations SET next_version_id=@new_record_id WHERE id=@old_record_id;
COMMIT;
END $$
DELIMITER ;
