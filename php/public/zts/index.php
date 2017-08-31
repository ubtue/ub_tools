<?php
    /**
     * Test site for Zotero Translation Server, using c++ zts_client and zotero_crawler
     */
    require('inc.php');
?>

<html>

<head>
    <link rel="stylesheet" href="style.css">
</head>
<body>
<h1>ZTS Test</h1>
<p><i>Zotero Translation Server</i></p>

<h2>Parameters</h2>
<form method="post" action="index.php">
    <table>
        <tr><td>Base Url</td><td><input name="UrlBase" type="text" value="<?= isset($_POST['UrlBase']) ? $_POST['UrlBase'] : 'http://allafrica.com/books/' ?>"></input></td><td>e.g. http://allafrica.com/books/</td></tr>
        <tr><td>Regex</td><td><input name="UrlRegex" type="text" value="<?= isset($_POST['UrlRegex']) ? $_POST['UrlRegex'] : '.*/books/.*' ?>"></input></td><td>e.g. .*/books/.*</td></tr>
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
        <tr><td colspan="3">Please note: This process can run for multiple minutes!</td></tr>
        <tr><td colspan="3"><input type="submit"></input></td></tr>
    </table>
</form>


<?php
if (count($_POST) > 0) {
    $IgnoreRobots = true;
    $zotero = new Zotero\MetadataHarvester(ZOTERO_SERVER_URL);
    $result = $zotero->Start($_POST['UrlBase'], $_POST['UrlRegex'], $_POST['Depth'], $IgnoreRobots, $_POST['FileExtension']);
    ?>
    <h2>Result</h2>
    <table>
        <tr><td>CMD</td><td><?= $result->Cmd ?></td></tr>
        <tr><td>CMD Exit Code</td><td style="color: <?= ($result->CmdStatus == 0) ? 'green' : 'red'?>;"><?= $result->CmdStatus ?></td></tr>
        <tr><td>CMD Duration</td><td><?= $result->Duration . 's' ?></td></tr>
        <tr><td>CMD Output</td><td><textarea rows="20" readonly="readonly"><?= implode("&#10;", $result->CmdOutput) ?></textarea></td></tr>
        <?php
            if ($result->CmdStatus == 0) {
                print '<tr><td>Download</td><td><a target="_blank" href="getresult.php?id=' . basename($result->MarcPath) . '">Result file</a></td></tr>';
            }
        ?>
    </table>

    <?php
}
?>

</body>
</html>