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

CREATE TABLE rss_aggregator (
    item_id VARCHAR(768) NOT NULL,
    item_url VARCHAR(1000) NOT NULL,
    item_title VARCHAR(1000) NOT NULL,
    item_description MEDIUMTEXT NOT NULL,
    serial_name VARCHAR(1000) NOT NULL,
    feed_url VARCHAR(1000) NOT NULL,
    pub_date DATETIME NOT NULL,
    insertion_time TIMESTAMP DEFAULT NOW() NOT NULL,
    CONSTRAINT rss_aggregator_item_id UNIQUE (item_id),
    INDEX rss_aggregator_item_id_index(item_id(768)),
    INDEX rss_aggregator_item_url_index(item_url(768)),
    INDEX rss_aggregator_insertion_time_index(insertion_time)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

CREATE TABLE zeder_journals (
    id INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
    zeder_id INT(11) UNSIGNED NOT NULL,
    zeder_instance ENUM('ixtheo', 'krimdok') NOT NULL,
    journal_name VARCHAR(1000) NOT NULL,
    PRIMARY KEY (id),
    CONSTRAINT zeder_journal_zeder_id_and_zeder_instance UNIQUE (zeder_id, zeder_instance)
) DEFAULT CHARSET=utf8mb4;

-- Table to be used w/ our validate_harvested_records tool:
CREATE TABLE metadata_presence_tracer (
    zeder_journal_id INT(11) UNSIGNED,
    marc_field_tag CHAR(3) NOT NULL,
    record_type ENUM('regular_article', 'review') DEFAULT 'regular_article',
    field_presence ENUM('always', 'sometimes', 'ignore') NOT NULL,
    CONSTRAINT metadata_presence_tracer_zeder_journal_id FOREIGN KEY (zeder_journal_id) REFERENCES zeder_journals (id) ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT metadata_presence_tracer_journal_id_and_field_name UNIQUE (zeder_journal_id, marc_field_tag)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;


CREATE TABLE delivered_marc_records (
    id INT AUTO_INCREMENT PRIMARY KEY,
    hash VARCHAR(40) NOT NULL,
    zeder_journal_id INT(11) UNSIGNED NOT NULL,
    delivery_state ENUM('automatic', 'manual', 'error') DEFAULT 'automatic' NOT NULL,
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
