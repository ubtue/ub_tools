CREATE TABLE IF NOT EXISTS rss_feeds (
    id INT AUTO_INCREMENT PRIMARY KEY,
    feed_url VARCHAR(512) NOT NULL,
    last_build_date DATETIME NOT NULL,
    last_modified DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) CHARACTER SET utf8mb4;
CREATE INDEX IF NOT EXISTS rss_feeds_ids_index ON rss_feeds(id);
CREATE INDEX IF NOT EXISTS rss_feeds_feed_url_index ON rss_feeds(feed_url);


CREATE TABLE IF NOT EXISTS rss_items (
    feed_id INT NOT NULL,
    item_id VARCHAR(512) NOT NULL,
    creation_datetime TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY feed_url_and_item_id(feed_id,item_id),
    CONSTRAINT feed_id FOREIGN KEY (feed_id) REFERENCES rss_feeds (id) ON DELETE CASCADE
) CHARACTER SET utf8mb4;

CREATE INDEX IF NOT EXISTS rss_items_feed_id_and_item_id_index ON rss_items(feed_id,item_id);
CREATE INDEX IF NOT EXISTS rss_items_creation_datetime_index ON rss_items(creation_datetime);
