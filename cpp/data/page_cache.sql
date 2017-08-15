CREATE TABLE cache (
    url_id SERIAL PRIMARY KEY,
    url VARCHAR(255) NOT NULL ,
    retrieval_datetime TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expiration_datetime TIMESTAMP NOT NULL DEFAULT NOW(),
    honor_robots_dot_txt BOOL NOT NULL DEFAULT TRUE,
    etag VARCHAR(128) NULL,
    http_header TEXT NOT NULL,
    status VARCHAR(255) NOT NULL DEFAULT 'ok',
    compressed_document_source BLOB NOT NULL,
    uncompressed_document_source_size INTEGER NOT NULL
);

CREATE INDEX cache_id ON cache(url);
CREATE INDEX cache_retrieval ON cache(retrieval_datetime);
CREATE INDEX cache_expiration ON cache(expiration_datetime);


CREATE TABLE redirect (
    url VARCHAR(255) NOT NULL PRIMARY KEY,
    cache_id INTEGER REFERENCES cache(url_id) ON DELETE CASCADE
);


CREATE TABLE anchors (
    anchor_id SERIAL PRIMARY KEY,
    anchor_text TEXT NOT NULL,
    url VARCHAR(255) NOT NULL,
    cache_id INTEGER REFERENCES cache(url_id) ON DELETE CASCADE
);
