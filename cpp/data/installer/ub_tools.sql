-- under CentOS we still have MariaDB 5, which has a limitation of 768 bytes for index keys.
-- this means we need to use a prefix length <= 768 when creating an index on a particular VARCHAR column.
-- Also, the default collating sequence is a Swedish one.  This leads to aliasing problems for characters with
-- diacritical marks => we need to override it and use utf8mb4_bin.

-- The sizes here must be in sync with the constants defined in rss_aggregator.cc!
CREATE TABLE database_versions (
    version INT UNSIGNED NOT NULL,
    database_name VARCHAR(64) NOT NULL,
    UNIQUE (database_name)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

CREATE TABLE rss_aggregator (
    item_id VARCHAR(768) NOT NULL,
    item_url VARCHAR(1000) NOT NULL,
    item_title VARCHAR(1000) NOT NULL,
    item_description TEXT NOT NULL,
    serial_name VARCHAR(1000) NOT NULL,
    feed_url VARCHAR(1000) NOT NULL,
    pub_date DATETIME NOT NULL,
    insertion_time TIMESTAMP DEFAULT NOW() NOT NULL,
    UNIQUE (item_id),
    INDEX item_id_index(item_id(768)),
    INDEX item_url_index(item_url(768)),
    INDEX insertion_time_index(insertion_time)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

-- Table to be used w/ our validate_harvested_records tool:
CREATE TABLE metadata_presence_tracer (
    journal_name VARCHAR(764) NOT NULL,
    metadata_field_name CHAR(4) NOT NULL,
    field_presence ENUM('always', 'sometimes', 'ignore') NOT NULL,
    UNIQUE(journal_name, metadata_field_name),
    INDEX journal_name_and_metadata_field_name_index(journal_name, metadata_field_name)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;


CREATE TABLE delivered_marc_records (
    id INT AUTO_INCREMENT PRIMARY KEY,
    hash VARCHAR(40) NOT NULL,
    zeder_id VARCHAR(10) NOT NULL,
    delivered_at TIMESTAMP NOT NULL DEFAULT NOW(),
    journal_name VARCHAR(1000) NOT NULL,
    main_title VARCHAR(1000) NOT NULL,
    publication_year CHAR(4) DEFAULT NULL,
    volume VARCHAR(20) DEFAULT NULL,
    issue VARCHAR(20) DEFAULT NULL,
    pages VARCHAR(20) DEFAULT NULL,
    resource_type ENUM('print','online','unknown') NOT NULL,
    record BLOB NOT NULL,
    INDEX delivered_marc_records_hash_index(hash),
    INDEX delivered_marc_records_zeder_id_index(zeder_id),
    INDEX delivered_marc_records_delivered_at_index(delivered_at),
    INDEX delivered_marc_records_journal_name_index(journal_name(768)),
    INDEX delivered_marc_records_main_title_index(main_title(768))
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

CREATE TABLE delivered_marc_records_superior_info (
    zeder_id VARCHAR(10) PRIMARY KEY,
    control_number VARCHAR(20) DEFAULT NULL,
    title VARCHAR(1000) NOT NULL,
    CONSTRAINT zeder_id FOREIGN KEY (zeder_id) REFERENCES delivered_marc_records (zeder_id) ON DELETE CASCADE ON UPDATE CASCADE
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

CREATE TABLE delivered_marc_records_urls (
    record_id INT,
    url VARCHAR(767) NOT NULL,
    CONSTRAINT PK_record_url PRIMARY KEY (record_id, url),
    CONSTRAINT FK_record_id FOREIGN KEY (record_id) REFERENCES delivered_marc_records (id) ON DELETE CASCADE ON UPDATE CASCADE,
    INDEX delivered_marc_records_urls_index(url)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
