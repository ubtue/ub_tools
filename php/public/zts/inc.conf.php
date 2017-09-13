<?php

// this is the main configuration file, containing directories and so on

// internal PHP library
define('DIR_LIB', '/usr/local/ub_tools/php/lib/');

// tmp directory (for CLI processing + downloads)
define('DIR_TMP', '/tmp/Zts/');

// max script runtime in seconds
define('MAX_SCRIPT_RUNTIME', 300);
ini_set('max_input_time', MAX_SCRIPT_RUNTIME);
ini_set('max_execution_time', MAX_SCRIPT_RUNTIME);

// Musterdatei für Zotero Crawler:
define('ZOTERO_FILE_CRAWLER', '/usr/local/ub_tools/cpp/data/zotero_crawler.conf');

// URL to Zotero server for zts_client
define('ZOTERO_SERVER_URL', 'http://ub28.uni-tuebingen.de:1969/web');

// Create own tempdir if not exists
if (!is_dir(DIR_TMP)) mkdir(DIR_TMP);