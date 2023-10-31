#!/bin/bash
set -o nounset -o pipefail

function test_mysql_access {
sudo mysql <<MYSQL
   USE mysql;
MYSQL
echo $?
}


function get_target_database {
    if [ ${TUEFIND_FLAVOUR} == "ixtheo" ]; then
        echo "ixtheo"
    else if [ ${TUEFIND_FLAVOUR} == "krimdok" ]; then
        echo "krim_translations"
    else
        echo "TUEFIND_FLAVOUR not set - skipping execution"
        exit 0
        fi
    fi
}


read -r -d '' UPDATE_PROCEDURE <<-MYSQL
DELIMITER //
DROP PROCEDURE IF EXISTS insert_vufind_translation_entry //
CREATE PROCEDURE insert_vufind_translation_entry(IN in_token VARCHAR(191), IN in_language_code CHAR(4), IN in_translation VARCHAR(1024), IN in_translator VARCHAR(50))
SQL SECURITY INVOKER
BEGIN

DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
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
END //


DROP PROCEDURE IF EXISTS insert_keyword_translation_entry //
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
          SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = @fullerror, MYSQL_ERRNO = 1614;
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
END //
DELIMITER ;
MYSQL

#############################################################
if [ ${EUID} -ne 0 ]; then
    echo "$0 must be run as root"
    exit 1
fi

result=$(test_mysql_access)
if [ $result == 0 ]; then
    sudo mysql --verbose --verbose $(get_target_database) < <(printf "%s" "${UPDATE_PROCEDURE}")
else
    read -p "Enter MySQL root password: " -s root_password
    export MYSQL_PWD="${root_password}"
    mysql --verbose --verbose -u root  \
       $(get_target_database) \
       < <(printf "%s" "${UPDATE_PROCEDURE}")
fi
echo "Finished..."
