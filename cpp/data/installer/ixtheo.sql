-- under CentOS we still have MariaDB 5, which has a limitation of 767 bytes for keys.
-- this means e.g. for VARCHAR with utf8mb4, we can use at most a VARCHAR(191)!
-- Also, the default collating sequence is a Swedish one.  This leads to aliasing problems for characters with
-- diacritical marks => we need to override it and use utf8mb4_bin.

CREATE TABLE vufind_translations (
  token VARCHAR(191) NOT NULL,
  language_code CHAR(4) NOT NULL,
  translation VARCHAR(1024) NOT NULL,
  translator VARCHAR(50),
  UNIQUE KEY token_language_code (token,language_code),
  KEY vufind_translations_idx_token (token),
  KEY vufind_translations_idx_language_code (language_code)
) DEFAULT CHARSET=utf8mb4;


CREATE TABLE keyword_translations (
  ppn CHAR(10) NOT NULL,
  gnd_code CHAR(10) NOT NULL,
  language_code CHAR(4) NOT NULL,
  translation VARCHAR(1024) NOT NULL,
  status ENUM('reliable_synonym', 'reliable', 'unreliable', 'unreliable_synonym', 'replaced', 'replaced_synonym',
              'new', 'new_synonym', 'macs') NOT NULL,
  origin CHAR(3) NOT NULL,
  gnd_system VARCHAR(30),
  translator VARCHAR(50),
  german_updated TINYINT(1),
  KEY keyword_translations_idx_ppn (ppn),
  KEY keyword_translations_idx_language_code (language_code),
  KEY keyword_translations_idx_translation (translation(30)),
  KEY keyword_translations_idx_gnd_code (gnd_code),
  KEY keyword_translations_idx_status (status),
  UNIQUE KEY ppn_language_code_status_translator (ppn, language_code, status, translator)
) DEFAULT CHARSET=utf8mb4;

CREATE TABLE translators (
  translator VARCHAR(30) NOT NULL,
  translation_target VARCHAR(20) NOT NULL,
  lookfor VARCHAR(100),
  filtered_lookfor VARCHAR(100),
  offset VARCHAR(10),
  filtered_offset VARCHAR(10),
  PRIMARY KEY (translator, translation_target)
) DEFAULT CHARSET=utf8mb4;

