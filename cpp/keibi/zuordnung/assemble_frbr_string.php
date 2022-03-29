<?php
if ($argc != 3) {
   print("Invalid number of arguments: " . $argc);
   exit(1);
}

$id = $argv[1];
$work_keys_str_mv = json_decode($argv[2]);
$query = [];
if (empty($work_keys_str_mv))
    exit(0);
foreach ($work_keys_str_mv as $work_key_string_mv) {
    // c.f. VuFindSearch/src/VuFindSearch/Backend/Solr/Backend.php
    $key = addcslashes($work_key_string_mv, '+-&|!(){}[]^"~*?:\\/');
    $query[] = "work_keys_str_mv:(\"$key\")";
}
$query_string = "q=" . urlencode(implode(' OR ', $query));
$query_string .= '&fq=' . sprintf('-id:"%s"', addcslashes($id, '"'));
echo $query_string;

?>
