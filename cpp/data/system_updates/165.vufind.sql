ALTER TABLE vufind.tuefind_user_authorities DROP KEY user_authority;
ALTER TABLE vufind.tuefind_user_authorities ADD CONSTRAINT user_authority UNIQUE (user_id, authority_id);
