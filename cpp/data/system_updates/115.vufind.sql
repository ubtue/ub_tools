ALTER TABLE user ADD tuefind_rights SET('admin', 'user_authorities') DEFAULT NULL AFTER tuefind_is_admin;
ALTER TABLE user DROP COLUMN tuefind_is_admin;
