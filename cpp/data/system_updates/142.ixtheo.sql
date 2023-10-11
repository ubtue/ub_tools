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
          SELECT @p1, @p2, @p3, @p4, @p5;
          ROLLBACK;
          SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'ERROR: statement could not be processed';
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
          SELECT @p1, @p2, @p3, @p4, @p5;
          ROLLBACK;
          SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'ERROR: statement could not be processed';
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
