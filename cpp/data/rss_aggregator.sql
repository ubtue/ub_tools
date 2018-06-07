CREATE TABLE IF NOT EXISTS rss_aggregator (
    item_url VARCHAR(512) NOT NULL,
    insertion_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
    UNIQUE (item_url)
) CHARACTER SET utf8mb4;
CREATE INDEX IF NOT EXISTS item_url_index ON rss_aggregator(item_url);
CREATE INDEX IF NOT EXISTS insertion_time_index ON rss_aggregator(insertion_time);
