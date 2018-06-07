-- The sizes here must be in sync with the constants defined in rss_aggregator.cc!
CREATE TABLE IF NOT EXISTS rss_aggregator (
    item_id VARCHAR(100) NOT NULL,
    item_url VARCHAR(512) NOT NULL,
    title_and_or_description TEXT NOT NULL,
    serial_name VARCHAR(200) NOT NULL,
    insertion_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
    UNIQUE (item_id)
) CHARACTER SET utf8mb4;
CREATE INDEX IF NOT EXISTS item_id_index ON rss_aggregator(item_id);
CREATE INDEX IF NOT EXISTS item_url_index ON rss_aggregator(item_url);
CREATE INDEX IF NOT EXISTS insertion_time_index ON rss_aggregator(insertion_time);
