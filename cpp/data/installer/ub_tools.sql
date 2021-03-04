-- under CentOS we still have MariaDB 5, which has a limitation of 768 bytes for index keys.
-- this means we need to use a prefix length <= 768 when creating an index on a particular VARCHAR column.
-- Also, the default collating sequence is a Swedish one.  This leads to aliasing problems for characters with
-- diacritical marks => we need to override it and use utf8mb4_bin.

-- The sizes here must be in sync with the constants defined in rss_aggregator.cc!

-- Please always add names to your CONSTRAINTs! (for succeeding sql_updates)
-- <table>_<field_names_with_"and"> (unless it's too long)
CREATE TABLE database_versions (
    version INT UNSIGNED NOT NULL,
    database_name VARCHAR(64) NOT NULL,
    CONSTRAINT database_versions_database_name UNIQUE (database_name)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

CREATE TABLE zeder_journals (
    id INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
    zeder_id INT(11) UNSIGNED NOT NULL,
    zeder_instance ENUM('ixtheo', 'krimdok') NOT NULL,
    journal_name VARCHAR(1000) NOT NULL,
    PRIMARY KEY (id),
    CONSTRAINT zeder_journal_zeder_id_and_zeder_instance UNIQUE (zeder_id, zeder_instance)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

-- Table to be used w/ our validate_harvested_records tool:
CREATE TABLE metadata_presence_tracer (
    journal_id INT(11) UNSIGNED,
    marc_field_tag CHAR(3) NOT NULL,
    marc_subfield_code CHAR(1) NOT NULL,
    regex VARCHAR(200),
    record_type ENUM('regular_article', 'review') DEFAULT 'regular_article' NOT NULL,
    field_presence ENUM('always', 'sometimes', 'ignore') NOT NULL,
    CONSTRAINT metadata_presence_tracer_journal_id FOREIGN KEY (journal_id) REFERENCES zeder_journals (id) ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT mpt_journal_id_marc_field_tag_and_subfield_code_and_record_type UNIQUE (journal_id, marc_field_tag, marc_subfield_code, record_type)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;


CREATE TABLE delivered_marc_records (
    id INT AUTO_INCREMENT PRIMARY KEY,
    hash VARCHAR(40) NOT NULL,
    zeder_journal_id INT(11) UNSIGNED NOT NULL,
    delivery_state ENUM('automatic', 'manual', 'error', 'ignore', 'reset', 'online_first') DEFAULT 'automatic' NOT NULL,
    error_message VARCHAR(1000) DEFAULT NULL,
    delivered_at TIMESTAMP NOT NULL DEFAULT NOW(),
    main_title VARCHAR(1000) NOT NULL,
    record BLOB NOT NULL,
    CONSTRAINT delivered_marc_records_zeder_journal_id FOREIGN KEY (zeder_journal_id) REFERENCES zeder_journals (id) ON DELETE CASCADE ON UPDATE CASCADE,
    INDEX delivered_marc_records_hash_index(hash),
    INDEX delivered_marc_records_delivery_state_index(delivery_state),
    INDEX delivered_marc_records_delivered_at_index(delivered_at),
    INDEX delivered_marc_records_main_title_index(main_title(768))
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;


-- Do not use utf8mb4 for URL's in big constraints, especially not if
-- combined with other fields
-- (Avoid error "Specified key was too long; max key length is 3072 bytes")
CREATE TABLE delivered_marc_records_urls (
    record_id INT,
    url VARCHAR(1000) CHARACTER SET 'utf8' COLLATE 'utf8_bin' NOT NULL,
    CONSTRAINT record_id_and_url PRIMARY KEY (record_id, url),
    CONSTRAINT delivered_marc_records_id FOREIGN KEY (record_id) REFERENCES delivered_marc_records (id) ON DELETE CASCADE ON UPDATE CASCADE,
    INDEX delivered_marc_records_urls_index(url)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
