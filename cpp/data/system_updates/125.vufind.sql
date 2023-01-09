ALTER TABLE vufind.user ADD tuefind_institution VARCHAR(255) DEFAULT NULL AFTER last_language;
ALTER TABLE vufind.user DROP INDEX tuefind_subscribed_to_newsletter_index;
ALTER TABLE vufind.user CHANGE tuefind_subscribed_to_newsletter krimdok_subscribed_to_newsletter BOOLEAN NOT NULL DEFAULT FALSE AFTER tuefind_rights;
CREATE INDEX krimdok_subscribed_to_newsletter_index ON vufind.user (krimdok_subscribed_to_newsletter);
