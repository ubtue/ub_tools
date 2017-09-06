<?php


namespace Zotero;

/**
 * Class for the Zotero Translation Server (using c++ zts_client and zotero_crawler)
 */
class MetadataHarvester {

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
     * @return \Zotero\Operation
     */
    public function Start($UrlBase, $UrlRegex, $Depth, $IgnoreRobots, $FileExtension) {
        $uniqid = uniqid('Zts_' . date('Y-m-d_H-i-s_'));
        $CfgPath = DIR_TMP . $uniqid . '.conf';

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
        $OutPath = DIR_TMP . $uniqid . '.' . $FileExtension;
        $ProgressPath = DIR_TMP . $uniqid . '.progress';

        self::_writeConfigFile($CfgPath, $UrlBase, $UrlRegex, $Depth);
        return $this->_executeCommand($uniqid, $CfgPath, $DirMapLocal, $OutPath, $IgnoreRobots);
    }

    /**
     * Get progress of the operation
     *
     * @param string $OperationId
     * @return boolean
     */
    static public function GetProgress($OperationId) {
        $path = self::_getProgressPath($OperationId);
        if (is_file($path)) {
            $progress_raw = file_get_contents($path);
            if ($progress_raw !== false && $progress_raw !== '') {
                $progress_percent = intval($progress_raw * 100);
                return $progress_percent;
            } else {
                return false;
            }
        }

        return false;
    }

    /**
     * Call zts_client (start operation, dont wait for result)
     *
     * @param string $Id
     * @param string $CfgPath
     * @param string $DirMap
     * @param string $OutPath
     * @param bool $IgnoreRobots
     * @return \Zotero\Operation
     */
    protected function _executeCommand($Id, $CfgPath, $DirMap, $OutPath, $IgnoreRobots=false) {
        $ProgressPath = self::_getProgressPath($Id);

        $cmd = 'zts_client';
        if ($IgnoreRobots) {
            $cmd .= ' --ignore-robots-dot-txt';
        }
        $cmd .= ' --zotero-crawler-config-file="' . $CfgPath . '"';
        if ($ProgressPath != null) {
            $cmd .= ' --progress-file="' . $ProgressPath . '"';
        }
        $cmd .= ' ' . $this->Url . ' ' . $DirMap . ' "' . $OutPath . '"';
        $cmd .= ' 2>&1';

        $operation = new Operation();
        $operation->Cmd = $cmd;
        $operation->MarcPath = $OutPath;
        $operation->OperationId = $Id;
        $descriptorspec = array(0 => array("pipe", "r"),  // stdin is a pipe that the child will read from
                                1 => array("pipe", "w"),  // stdout is a pipe that the child will write to
                                2 => array("pipe", "w"),  // stderr is a pipe that the child will write to);
                                );
        $operation->Resource = proc_open($cmd, $descriptorspec, $operation->Pipes);

        return $operation;
    }

    /**
     * Get path to progress file (tmp) for this operation
     *
     * @param type $OperationId
     * @return string
     */
    static protected function _getProgressPath($OperationId) {
        return DIR_TMP . $OperationId . '.progress';
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
 * Operation class, generated as soon as the shell command is executed.
 * Can be used to monitor the status of the running cli subprocess.
 */
class Operation {
    /**
     * contains the full CLI call (for debug output)
     * @var string
     */
    public $Cmd;

    /**
     * Path to output file
     * @var string
     */
    public $MarcPath;

    /**
     * Operation Id. Unique string, can be used for status requests
     * @var string
     */
    public $OperationId;

    /**
     * array of pipes opened to the process. see www.php.net/proc_open
     * @var array
     */
    public $Pipes;

    /**
     * Resource (e.g. for proc_get_status)
     * @var type
     */
    public $Resource;

    /**
     * destructor: close pipes if still open
     */
    function __destruct() {
        @fclose($this->Pipes[0]);
        @fclose($this->Pipes[1]);
        @fclose($this->Pipes[2]);
    }

    /**
     * Get CLI output
     * we only read stdout, because stderr was redirected to stdout in _executeCommand
     *
     * @return string
     */
    public function GetOutput() {
        return stream_get_contents($this->Pipes[1]);
    }

    /**
     * Get progress (in percent)
     * @return int
     */
    public function GetProgress() {
        return MetadataHarvester::GetProgress($this->OperationId);
    }

    /**
     * Get status (see www.php.net/proc_get_status)
     */
    public function GetStatus() {
        return proc_get_status($this->Resource);
    }
}