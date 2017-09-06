<?php
    /**
     * Test site for Zotero Translation Server, using c++ zts_client and zotero_crawler
     */
    require('inc.php');

    // Helper functions for javascript
    function updateProgressBar($progress) {
        print '<script type="text/javascript">updateProgressBar('.$progress.');</script>';
        ob_flush_real();
    }

    function updateRuntime($seconds) {
        print '<script type="text/javascript">updateRuntime('.$seconds.');</script>';
        ob_flush_real();
    }

?>
<!DOCTYPE HTML>
<html>
<head>
    <link rel="stylesheet" href="../res/bootstrap4/css/bootstrap.min.css"/>
    <link rel="stylesheet" href="style.css"/>
    <script type="text/javascript" src="../res/jquery/jquery-3.2.1.min.js"></script>
    <script type="text/javascript">
        function updateProgressBar(percent) {
            var progressBar = document.getElementById('progress-bar');
            progressBar.setAttribute('aria-valuenow', percent);
            progressBar.style.width = percent + '%';
            progressBar.innerHTML = percent + '% Complete';
        }

        function updateRuntime(seconds) {
            var div_runtime = document.getElementById('runtime');
            div_runtime.innerHTML = seconds + 's';
        }
    </script>
</head>
<body>
<h1>ZTS Test</h1>
<p><i>Zotero Translation Server</i></p>

<h2>Parameters</h2>
<form method="post" action="index.php">
    <table>
        <tr><td>Base Url</td><td><input name="UrlBase" type="text" value="<?= isset($_POST['UrlBase']) ? $_POST['UrlBase'] : 'https://www.nationalarchives.gov.uk/first-world-war' ?>"></input></td><td>e.g. http://allafrica.com/books/</td></tr>
        <tr><td>Regex</td><td><input name="UrlRegex" type="text" value="<?= isset($_POST['UrlRegex']) ? $_POST['UrlRegex'] : '.*/first-world-war/.*' ?>"></input></td><td>e.g. .*/books/.*</td></tr>
        <tr>
            <td>Depth</td>
            <td>
                <select name="Depth">
                    <?php
                        $default = 2;
                        if (isset($_POST['Depth'])) $default = $_POST['Depth'];
                        for ($i=1;$i<=5;$i++) {
                            if ($i == $default) {
                                print '<option selected="selected">'.$i.'</option>';
                            } else {
                                print '<option>'.$i.'</option>';
                            }
                        }
                    ?>
                </select>
            </td>
            <td></td>
        </tr>
        <tr>
            <td>Format</td>
            <td>
                <select name="FileExtension">
                    <option value="xml">MARCXML</option>
                    <!--<option value="mrc" <?= (isset($_POST['FileExtension']) && $_POST['FileExtension'] == 'mrc') ? 'selected' : '' ?>>MARC21</option>-->
                </select>
            </td>
            <td>MARC21 currently disabled due to problems with zts_client</td>
        </tr>
        <tr><td colspan="3">Please note: This process can run for multiple minutes! Max runtime: <?= MAX_SCRIPT_RUNTIME ?>s</td></tr>
        <tr><td colspan="3"><input type="submit"></input></td></tr>
    </table>
</form>

<?php
if (count($_POST) > 0) {
    $IgnoreRobots = true;
    $zotero = new Zotero\MetadataHarvester(ZOTERO_SERVER_URL);
    $task = $zotero->start($_POST['UrlBase'], $_POST['UrlRegex'], $_POST['Depth'], $IgnoreRobots, $_POST['FileExtension']);
    ?>
    <h2>Result</h2>
    <table>
        <tr><td>Command</td><td><?= $task->Cmd ?></td></tr>
        <tr><td>Runtime</td><td id="runtime"></td></tr>
        <tr><td>Progress</td><td><div class="progress"><div id="progress-bar" class="progress-bar" role="progressbar" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100" style="width:0%">0% Complete</div></div></td></tr>

    <?php

    // send already generated output to browser
    ob_flush_real();

    // start status monitoring
    $progress = 0;
    $progress_old = false;
    $starttime = time();
    $status = null;

    do {
        sleep(1);
        updateRuntime(time() - $starttime);
        $progress = $task->getProgress();
        if ($progress !== false && $progress !== $progress_old) {
            updateProgressBar($progress);
            $progress_old = $progress;
        }
        $status = $task->getStatus();
    } while ($status['running']);

    if ($status['exitcode'] == 0) {
        print '<tr><td>Download</td><td><a target="_blank" href="getresult.php?id=' . basename($task->MarcPath) . '">Result file</a></td></tr>';
    } else {
        print '<tr><td>ERROR</td><td>Exitcode: '.$status['exitcode'].'</td></tr>';
    }

    print '<tr><td>CLI output:</td><td>'.nl2br($task->getOutput()).'</td></tr>';

    ?>
    </table>

    <?php
}
?>

</body>
</html>