<?php


namespace Zotero;

/**
 * Class for the Zotero Translation Server (using c++ zts_client and zotero_crawler)
 */
class Server {

    /**
     * Main directory for zts client maps
     */
    const DIR_ZTS_CLIENT_MAPS = '/usr/local/ub_tools/cpp/data/zts_client_maps/';

    /**
     * Example config file for crawler
     */
    const FILE_CRAWLER_EXAMPLE = '/usr/local/ub_tools/cpp/data/zotero_crawler.conf';

    /**
     * URL of Server
     * @var string
     */
    protected $Url;

    /**
     * Initialize new Server
     * @param string $Url
     */
    public function __construct($Url) {
        $this->Url = $Url;
    }

    /**
     * Start crawling, using zts_client
     *
     * @param string $UrlBase
     * @param string $UrlRegex
     * @param int $Depth
     * @param bool $IgnoreRobots
     * @param string $FileExtension     supported extension, e.g. "xml" for MARCXML or "mrc" for MARC21
     * @return \Zotero\Result
     */
    public function Start($UrlBase, $UrlRegex, $Depth, $IgnoreRobots, $FileExtension) {
        $CfgPath = DIR_TMP . uniqid('Zts_') . '.conf';

        // generate local copy of zts_client_maps
        $DirMap = self::DIR_ZTS_CLIENT_MAPS;
        $DirMapLocal = DIR_TMP . 'ZtsMap/';
        if (!is_dir($DirMapLocal)) {
            copy_recursive($DirMap, $DirMapLocal);

            // reset previously_downloaded.hashes
            $FilePrevDownloaded = $DirMapLocal . 'previously_downloaded.hashes';
            unlink($FilePrevDownloaded);
            symlink('/dev/null', $FilePrevDownloaded);
        }

        // only .mrc or .xml (type will be auto detected)
        $OutPath = DIR_TMP . uniqid('Zts_' . date('Y-m-d H-i-s')) . '.' . $FileExtension;

        self::_writeConfigFile($CfgPath, $UrlBase, $UrlRegex, $Depth);
        $result = $this->_executeCommand($CfgPath, $DirMapLocal, $OutPath, $IgnoreRobots);

        // cleanup
        unlink($CfgPath);

        return $result;
    }

    /**
     * Call zts_client
     *
     * @param string $CfgPath
     * @param string $DirMap
     * @param string $OutPath
     * @param bool $IgnoreRobots
     * @return \Zotero\Result
     */
    protected function _executeCommand($CfgPath, $DirMap, $OutPath, $IgnoreRobots=false) {
        $starttime = time();
        $cmd = 'zts_client';
        if ($IgnoreRobots) {
            $cmd .= ' --ignore-robots-dot-txt';
        }
        $cmd .= ' --zotero-crawler-config-file=' . $CfgPath . ' ' . $this->Url . ' ' . $DirMap . ' "' . $OutPath . '"';
        $cmd .= ' 2>&1';

        $result = new Result();
        $result->Cmd = $cmd;
        exec($cmd, $result->CmdOutput, $result->CmdStatus);
        $result->MarcPath = $OutPath;
        $result->Duration = time() - $starttime;

        return $result;
    }

    /**
     * Prepare config file for zts_client
     *
     * @param string $CfgPath
     * @param string $UrlBase
     * @param string $UrlRegex
     * @param int $Depth
     */
    static protected function _writeConfigFile($CfgPath, $UrlBase, $UrlRegex, $Depth) {
        $CfgContents = '# start_URL max_crawl_depth URL_regex' . PHP_EOL;
        $CfgContents .= implode(' ', array($UrlBase, $Depth, $UrlRegex));
        file_put_contents($CfgPath, $CfgContents);
    }
}

/**
 * Result class for zts_client
 */
class Result {
    /**
     * contains the full CLI call (for debug output)
     * @var string
     */
    public $Cmd;

    /**
     * Contains the exit code
     * @var int
     */
    public $CmdStatus;

    /**
     * Contains the full command line output as array of string
     * @var array
     */
    public $CmdOutput;

    /**
     * Duration in seconds that the script needed to run
     * @var int
     */
    public $Duration;

    /**
     * Path to output file
     * @var string
     */
    public $MarcPath;
}