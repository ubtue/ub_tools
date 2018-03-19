CREATE TABLE rss (
    server_url VARCHAR(512) NOT NULL,
    item_id VARCHAR(512) NOT NULL,
    creation_datetime TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY server_url_and_item_id(server_url,item_id)
);

CREATE INDEX server_url_index ON rss(server_url);
CREATE INDEX server_url_and_item_id_index ON rss(server_url,item_id);
CREATE INDEX creation_datetime_index ON rss(creation_datetime);
