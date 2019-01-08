-- under CentOS we still have MariaDB 5, which has a limitation of 767 bytes for keys.
-- this means e.g. for VARCHAR with utf8mb4, we can use at most a VARCHAR(191)!
-- Also, the default collating sequence is a Swedish one.  This leads to aliasing problems for characters with
-- diacritical marks => we need to override it and use utf8mb4_bin.

-- The sizes here must be in sync with the constants defined in rss_aggregator.cc!
CREATE TABLE rss_aggregator (
    item_id VARCHAR(191) NOT NULL,
    item_url VARCHAR(512) NOT NULL,
    title_and_or_description TEXT NOT NULL,
    serial_name VARCHAR(200) NOT NULL,
    insertion_time TIMESTAMP DEFAULT NOW() NOT NULL,
    UNIQUE (item_id)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
CREATE INDEX item_id_index ON rss_aggregator(item_id);
CREATE INDEX item_url_index ON rss_aggregator(item_url);
CREATE INDEX insertion_time_index ON rss_aggregator(insertion_time);


CREATE TABLE rss_feeds (
    id INT AUTO_INCREMENT PRIMARY KEY,
    feed_url VARCHAR(191) NOT NULL,
    last_build_date DATETIME NOT NULL,
    UNIQUE (feed_url)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
CREATE INDEX rss_feeds_ids_index ON rss_feeds(id);
CREATE INDEX rss_feeds_feed_url_index ON rss_feeds(feed_url);


CREATE TABLE rss_items (
    feed_id INT NOT NULL,
    item_id VARCHAR(191) NOT NULL,
    creation_datetime TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE KEY feed_url_and_item_id(feed_id,item_id),
    CONSTRAINT feed_id FOREIGN KEY (feed_id) REFERENCES rss_feeds (id) ON DELETE CASCADE
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
CREATE INDEX rss_items_feed_id_and_item_id_index ON rss_items(feed_id,item_id);
CREATE INDEX rss_items_creation_datetime_index ON rss_items(creation_datetime);


-- Table to be used w/ our find_missing_metadata tool:
CREATE TABLE metadata_presence_tracer (
       journal_name VARCHAR(191) NOT NULL,
       metadata_field_name VARCHAR(191) NOT NULL,
       field_presence ENUM('always', 'sometimes', 'ignore') NOT NULL,
       UNIQUE(journal_name, metadata_field_name)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
CREATE INDEX journal_name_and_metadata_field_name_index ON metadata_presence_tracer(journal_name, metadata_field_name);


CREATE TABLE harvested_urls (
    id INT AUTO_INCREMENT PRIMARY KEY,
    url VARCHAR(191) NOT NULL,
    delivery_mode ENUM('test', 'live') NOT NULL,
    last_harvest_time DATETIME NOT NULL,
    journal_name VARCHAR(191) NOT NULL,
    checksum CHAR(40),
    error_message VARCHAR(191),
    CONSTRAINT url_and_delivery_mode UNIQUE (url, delivery_mode)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
CREATE INDEX harvested_urls_id_and_journal_name_index on harvested_urls(id, journal_name);

CREATE TABLE marc_records (
    id INT AUTO_INCREMENT PRIMARY KEY,
    url VARCHAR(191) NOT NULL,
    hash VARCHAR(40) NOT NULL,
    zeder_id VARCHAR(10) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    main_title VARCHAR(191) NOT NULL,
    publication_year CHAR(4) DEFAULT NULL,
    volume CHAR(40) DEFAULT NULL,
    issue CHAR(40) DEFAULT NULL,
    pages CHAR(20) DEFAULT NULL,
    resource_type ENUM('print','online','unknown') NOT NULL,
    record BLOB NOT NULL
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
CREATE INDEX marc_records_url_index ON marc_records(url);
CREATE INDEX marc_records_hash_index ON marc_records(hash);
CREATE INDEX marc_records_zeder_id_index ON marc_records(zeder_id);
CREATE INDEX marc_records_created_at_index ON marc_records(created_at);
CREATE INDEX marc_records_main_title_index ON marc_records(main_title);

CREATE TABLE superior_info (
    zeder_id VARCHAR(10) PRIMARY KEY,
    control_number VARCHAR(20) DEFAULT NULL,
    title VARCHAR(191) NOT NULL,
    CONSTRAINT zeder_id FOREIGN KEY (zeder_id) REFERENCES marc_records (zeder_id) ON DELETE CASCADE
) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
