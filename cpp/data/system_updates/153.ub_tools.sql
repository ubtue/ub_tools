ALTER TABLE delivered_marc_records MODIFY COLUMN delivery_state ENUM('automatic', 'manual', 'error', 'ignore', 'reset', 'online_first', 'legacy') DEFAULT 'automatic' NOT NULL
