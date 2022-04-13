ALTER TABLE tuefind_user_authorities ADD COLUMN requested_datetime TIMESTAMP DEFAULT NOW() NOT NULL AFTER access_state;
ALTER TABLE tuefind_user_authorities ADD COLUMN granted_datetime TIMESTAMP DEFAULT NULL AFTER requested_datetime;
UPDATE tuefind_user_authorities SET granted_datetime = NOW() WHERE access_state = 'granted' AND granted_datetime IS NULL;
