<?php


namespace RSS;

/**
 * Class for Metadata Harvesting of RSS feeds (using C++ rss_harvester)
 */
class MetadataHarvester {
    /**
     * Mapping of format to file extension
     */
    const OUTPUT_FORMATS = [
        // custom formats
        'marc21'            => 'mrc',
        'marcxml'           => 'xml',
    ];

    /**
     * URL of Zotero Translation Server
     * @var string
     */
    protected $zoteroUrl;

    /**
     * Initialize new Harvester
     * @param string $zoteroUrl
     */
    public function __construct(string $zoteroUrl) {
        $this->zoteroUrl = $zoteroUrl;
    }

    /**
     * Start harvesting, using rss_harvester
     *
     * @param string $urlBase   RSS feed URL
     * @param string $format    see OUTPUT_FORMATS for valid formats (e.g. marcxml)
     * @return TaskResult
     */
    public function start(string $urlBase, string $format): TaskResult {
        $uniqid = uniqid('Zts_' . date('Y-m-d_H-i-s_'));

        // generate local copy of zts_client_maps
        $mapDir = \Zotero\MetadataHarvester::DIR_ZTS_CLIENT_MAPS;
        $mapDirLocal = DIR_TMP . 'ZtsMap/';
        if (!is_dir($mapDirLocal)) {
            copy_recursive($mapDir, $mapDirLocal);

            // reset previously_downloaded.hashes
            $filePrevDownloaded = $mapDirLocal . 'previously_downloaded.hashes';
            unlink($filePrevDownloaded);
            symlink('/dev/null', $filePrevDownloaded);
        }

        $fileExtension = self::OUTPUT_FORMATS[$format];
        $outPath = DIR_TMP . $uniqid . '.' . $fileExtension;
        $rssUrlPath = DIR_TMP . $uniqid . '.url';
        file_put_contents($rssUrlPath, $urlBase);

        return $this->_executeCommand($rssUrlPath, $mapDirLocal, $outPath);
    }

    /**
     * Call rss_harvester
     *
     * @param string $rssUrlFile    Path to file with RSS feed URL
     * @param string $mapDir        Path to map directory
     * @param string $outPath       Path to write out file
     * @return TaskResult
     */
    protected function _executeCommand(string $rssUrlFile, string $mapDir, string $outPath): TaskResult {
        $cmd = 'rss_harvester --test';
        if (ZOTERO_PROXY_SERVER != '') {
            $cmd .= ' "--proxy=' . ZOTERO_PROXY_SERVER . '"';
        }
        $cmd .= ' "' . $rssUrlFile . '"' . ' "' . $this->zoteroUrl . '" "' . $mapDir . '" "' . $outPath . '"';
        $cmd .= ' 2>&1';

        exec($cmd, $output, $exitCode);

        $result = new TaskResult();
        $result->cmd = $cmd;
        $result->exitCode = $exitCode;
        $result->output = implode("\n", $output);
        $result->outPath = $outPath;
        return $result;
    }
}


class TaskResult {
    public $cmd;
    public $exitCode;
    public $output;
    public $outPath;
}
