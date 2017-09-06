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
    protected $url;

    /**
     * Initialize new Server
     * @param string $url
     */
    public function __construct($url) {
        $this->url = $url;
    }

    /**
     * Start crawling, using zts_client
     *
     * @param string $urlBase
     * @param string $urlRegex
     * @param int $depth
     * @param bool $ignoreRobots
     * @param string $fileExtension     supported extension, e.g. "xml" for MARCXML or "mrc" for MARC21
     * @return \Zotero\BackgroundTask
     */
    public function start($urlBase, $urlRegex, $depth, $ignoreRobots, $fileExtension) {
        $uniqid = uniqid('Zts_' . date('Y-m-d_H-i-s_'));
        $cfgPath = DIR_TMP . $uniqid . '.conf';

        // generate local copy of zts_client_maps
        $dirMap = self::DIR_ZTS_CLIENT_MAPS;
        $dirMapLocal = DIR_TMP . 'ZtsMap/';
        if (!is_dir($dirMapLocal)) {
            copy_recursive($dirMap, $dirMapLocal);

            // reset previously_downloaded.hashes
            $filePrevDownloaded = $dirMapLocal . 'previously_downloaded.hashes';
            unlink($filePrevDownloaded);
            symlink('/dev/null', $filePrevDownloaded);
        }

        // only .mrc or .xml (type will be auto detected)
        $outPath = DIR_TMP . $uniqid . '.' . $fileExtension;

        self::_writeConfigFile($cfgPath, $urlBase, $urlRegex, $depth);
        return $this->_executeCommand($uniqid, $cfgPath, $dirMapLocal, $outPath, $ignoreRobots);
    }

    /**
     * Call zts_client (start background task, dont wait for result)
     *
     * @param string $taskId
     * @param string $cfgPath
     * @param string $dirMap
     * @param string $outPath
     * @param bool $ignoreRobots
     * @return \Zotero\BackgroundTask
     */
    protected function _executeCommand($taskId, $cfgPath, $dirMap, $outPath, $ignoreRobots=false) {
        $progressPath = BackgroundTask::getProgressPath($taskId);

        $cmd = 'zts_client';
        if ($ignoreRobots) {
            $cmd .= ' --ignore-robots-dot-txt';
        }
        $cmd .= ' --zotero-crawler-config-file="' . $cfgPath . '"';
        if ($progressPath != null) {
            $cmd .= ' --progress-file="' . $progressPath . '"';
        }
        $cmd .= ' ' . $this->url . ' ' . $dirMap . ' "' . $outPath . '"';
        $cmd .= ' 2>&1';

        $task = new BackgroundTask();
        $task->cmd = $cmd;
        $task->marcPath = $outPath;
        $task->taskId = $taskId;
        $descriptorspec = array(0 => array("pipe", "r"),  // stdin is a pipe that the child will read from
                                1 => array("pipe", "w"),  // stdout is a pipe that the child will write to
                                2 => array("pipe", "w"),  // stderr is a pipe that the child will write to);
                                );
        $task->resource = proc_open($cmd, $descriptorspec, $task->pipes);

        return $task;
    }

    /**
     * Prepare config file for zts_client
     *
     * @param string $cfgPath
     * @param string $urlBase
     * @param string $urlRegex
     * @param int $depth
     */
    static protected function _writeConfigFile($cfgPath, $urlBase, $urlRegex, $depth) {
        $CfgContents = '# start_URL max_crawl_depth URL_regex' . PHP_EOL;
        $CfgContents .= implode(' ', array($urlBase, $depth, $urlRegex));
        file_put_contents($cfgPath, $CfgContents);
    }
}

/**
 * Class for background tasks, generated as soon as the shell command is executed.
 * Can be used to monitor the status of the running cli subprocess.
 */
class BackgroundTask {
    /**
     * contains the full CLI call (for debug output)
     * @var string
     */
    public $cmd;

    /**
     * Path to output file
     * @var string
     */
    public $marcPath;

    /**
     * Task Id. Unique string, can be used for status requests
     * @var string
     */
    public $taskId;

    /**
     * array of pipes opened to the process. see www.php.net/proc_open
     * @var array
     */
    public $pipes;

    /**
     * Resource (e.g. for proc_get_status)
     * @var type
     */
    public $resource;

    /**
     * destructor: close pipes if still open
     */
    function __destruct() {
        @fclose($this->pipes[0]);
        @fclose($this->pipes[1]);
        @fclose($this->pipes[2]);
    }

    /**
     * Get CLI output
     * we only read stdout, because stderr was redirected to stdout in _executeCommand
     *
     * @return string
     */
    public function getOutput() {
        return stream_get_contents($this->pipes[1]);
    }

    /**
     * Get progress (in percent)
     * Might also be false, if the subprocess didnt write the progress file yet.
     * So it is recommended to treat false as 0 percent, and not as error.
     *
     * @return mixed        int (percentage) or false (error)
     */
    public function getProgress() {
        $path = self::getProgressPath($this->taskId);
        if (is_file($path)) {
            $progressRaw = file_get_contents($path);
            if ($progressRaw !== false && $progressRaw !== '') {
                $progressPercent = intval($progressRaw * 100);
                return $progressPercent;
            } else {
                return false;
            }
        }
    }

    /**
     * Get path to progress file (tmp) for a background task
     *
     * @param string $taskId
     * @return string
     */
    static public function getProgressPath($taskId) {
        return DIR_TMP . $taskId . '.progress';
    }

    /**
     * Get status (see www.php.net/proc_get_status)
     */
    public function getStatus() {
        return proc_get_status($this->resource);
    }
}