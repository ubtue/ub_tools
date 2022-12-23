UPDATE vufind.user SET tuefind_institution=ixtheo_institution WHERE ixtheo_institution IS NOT NULL AND ixtheo_institution != '';
ALTER TABLE vufind.user DROP COLUMN ixtheo_institution;
ALTER TABLE vufind.user DROP INDEX krimdok_subscribed_to_newsletter_index;
ALTER TABLE vufind.user DROP COLUMN krimdok_subscribed_to_newsletter;
