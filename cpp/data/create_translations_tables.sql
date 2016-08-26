CREATE TABLE `vufind_translations` (
  `token` varchar(255) NOT NULL,
  `language_code` char(3) NOT NULL,
  `translation` varchar(255) NOT NULL,
  UNIQUE KEY `token_language_code` (`token`,`language_code`),
  KEY `vufind_translations_idx_token` (`token`),
  KEY `vufind_translations_idx_language_code` (`language_code`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `keyword_translations` (
  `ppn` varchar(9) NOT NULL,
  `language_code` char(3) NOT NULL,
  `translation` varchar(255) NOT NULL,
  `preexists` tinyint(1) NOT NULL DEFAULT '0',
  KEY `keyword_translations_idx_id` (`id`),
  KEY `keyword_translations_idx_language_code` (`language_code`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
