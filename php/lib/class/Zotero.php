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
     * Mapping of format to file extension
     */
    const OUTPUT_FORMATS = [
        'json'      => 'json',
        'marc21'    => 'mrc',
        'marcxml'   => 'xml',
    ];

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
     * @param string $format     see zts_client for valid formats (e.g. json)
     * @return \Zotero\BackgroundTask
     */
    public function start($urlBase, $urlRegex, $depth, $format) {
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

        $fileExtension = MetadataHarvester::OUTPUT_FORMATS[$format];
        $outPath = DIR_TMP . $uniqid . '.' . $fileExtension;

        self::_writeConfigFile($cfgPath, $urlBase, $urlRegex, $depth);
        return $this->_executeCommand($uniqid, $cfgPath, $dirMapLocal, $format, $outPath);
    }

    /**
     * Call zts_client (start background task, dont wait for result)
     *
     * @param string $taskId
     * @param string $cfgPath
     * @param string $dirMap
     * @param string $outFormat
     * @param string $outPath
     * @return \Zotero\BackgroundTask
     */
    protected function _executeCommand($taskId, $cfgPath, $dirMap, $outFormat, $outPath) {
        $progressPath = BackgroundTask::getProgressPath($taskId);

        $cmd = 'zts_client';
        $cmd .= ' --simple-crawler-config-file="' . $cfgPath . '"';
        if ($progressPath != null) {
            $cmd .= ' --progress-file="' . $progressPath . '"';
        }
        $cmd .= ' --output-format=' . $outFormat;
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
     * Get progress information.
     *
     * Might also be false, if the subprocess didnt write the progress file yet.
     * So it is recommended to treat false as no progress, and not as error.
     *
     * @return array ['processed_url_count' => int, 'remaining_depth' => int, 'current_url' => string] or false (error)
     */
    public function getProgress() {
        $path = self::getProgressPath($this->taskId);
        if (is_file($path)) {
            $progressRaw = file_get_contents($path);
            if ($progressRaw !== false && $progressRaw !== '') {
                $progress = explode(';', $progressRaw);
                return ['processed_url_count' => intval($progress[0]),
                        'remaining_depth'     => intval($progress[1]),
                        'current_url'         => $progress[2],
                ];
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
