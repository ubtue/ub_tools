-- under CentOS we still have MariaDB 5, which has a limitation of 767 bytes for keys.
-- this means e.g. for VARCHAR with utf8mb4, we can use at most a VARCHAR(191)!
-- Also, the default collating sequence is a Swedish one.  This leads to aliasing problems for characters with
-- diacritical marks => we need to override it and use utf8mb4_bin.

-- The sizes here must be in sync with the constants defined in rss_aggregator.cc!
CREATE TABLE database_versions (
    version INT UNSIGNED NOT NULL,
    database_name VARCHAR(64) NOT NULL,
    UNIQUE (database_name)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

CREATE TABLE rss_aggregator (
    item_id VARCHAR(191) NOT NULL,
    item_url VARCHAR(512) NOT NULL,
    item_title VARCHAR(200) NOT NULL,
    item_description TEXT NOT NULL,
    serial_name VARCHAR(200) NOT NULL,
    feed_url VARCHAR(512) NOT NULL,
    pub_date DATETIME NOT NULL,
    insertion_time TIMESTAMP DEFAULT NOW() NOT NULL,
    UNIQUE (item_id),
    INDEX item_id_index(item_id),
    INDEX item_url_index(item_url),
    INDEX insertion_time_index(insertion_time)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

-- Table to be used w/ our validate_harvested_records tool:
CREATE TABLE metadata_presence_tracer (
       journal_name VARCHAR(191) NOT NULL,
       metadata_field_name VARCHAR(191) NOT NULL,
       field_presence ENUM('always', 'sometimes', 'ignore') NOT NULL,
       UNIQUE(journal_name, metadata_field_name),
       INDEX journal_name_and_metadata_field_name_index(journal_name, metadata_field_name)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;


CREATE TABLE delivered_marc_records (
    id INT AUTO_INCREMENT PRIMARY KEY,
    url VARCHAR(191) NOT NULL,
    hash VARCHAR(40) NOT NULL,
    zeder_id VARCHAR(10) NOT NULL,
    delivered_at TIMESTAMP NOT NULL DEFAULT NOW(),
    journal_name VARCHAR(191) NOT NULL,
    main_title VARCHAR(191) NOT NULL,
    publication_year CHAR(4) DEFAULT NULL,
    volume CHAR(40) DEFAULT NULL,
    issue CHAR(40) DEFAULT NULL,
    pages CHAR(20) DEFAULT NULL,
    resource_type ENUM('print','online','unknown') NOT NULL,
    record BLOB NOT NULL,
    INDEX delivered_marc_records_url_index(url),
    INDEX delivered_marc_records_hash_index(hash),
    INDEX delivered_marc_records_zeder_id_index(zeder_id),
    INDEX delivered_marc_records_delivered_at_index(delivered_at),
    INDEX delivered_marc_records_journal_name_index(journal_name),
    INDEX delivered_marc_records_main_title_index(main_title)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

CREATE TABLE delivered_marc_records_superior_info (
    zeder_id VARCHAR(10) PRIMARY KEY,
    control_number VARCHAR(20) DEFAULT NULL,
    title VARCHAR(191) NOT NULL,
    CONSTRAINT zeder_id FOREIGN KEY (zeder_id) REFERENCES delivered_marc_records (zeder_id) ON DELETE CASCADE ON UPDATE CASCADE
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
