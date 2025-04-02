<?php
/** \file dspace_uploader.php
 *  \brief A tool for creating DSpace records and uploading fulltexts
 *         based on a PPN to fulltext file list
 *
 *  \copyright 2016-2021 Universitätsbibliothek Tübingen.  All rights reserved.
 *  Based on code generated by the you.com chatbot
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

require 'Symfony/Component/Intl/autoload.php';

use Symfony\Component\Intl\Languages;

function GetConfig() {
    $configFile = 'dspace_uploader.ini';
    if (!file_exists($configFile))
        die("Configuration file " . $configFile . " not found.\n");

    $config = parse_ini_file($configFile);
    if (!$config || !isset($config['dspace_username'], $config['dspace_password'], $config['dspace_api_url'],
            $config['dspace_collection_uuid']))
        die("Invalid configuration file. Ensure it contains 'dspace_username', 'dspace_password', 'dspace_api_url' and
            dspace_collection_uuid . \n");
    return $config;
}


function GetSessionToken($config) {
    $username = $config['dspace_username'];
    $password = $config['dspace_password'];
    $dspaceApiUrl = rtrim($config['dspace_api_url'], '/');

    // Authenticate with the DSpace REST API
    $authUrl = $dspaceApiUrl . '/login';
    $ch = curl_init($authUrl);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_POSTFIELDS, http_build_query(['email' => $username, 'password' => $password]));
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, true);

    $response = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    if ($httpCode !== 200) {
        die("Authentication failed. Please check your credentials.\n");
    }

    // Extract the session token from the response headers
    preg_match('/^Set-Cookie:\s*JSESSIONID=([^;]*)/mi', $response, $matches);
    if (!isset($matches[1])) {
        die("Failed to retrieve session token.\n");
    }
    $sessionToken = $matches[1];
    echo "Authentication successful. Session token: $sessionToken\n";
    return $sessionToken;
}


function GetMetadataFromK10Plus($ppn) {
    $url = 'https://sru.k10plus.de/opac-de-627?version=1.1&operation=searchRetrieve&query=pica.ppn%3D' . $ppn .
        '&maximumRecords=1&recordSchema=marcxml';

    $sruResponse = file_get_contents($url);
    $xml = simplexml_load_string($sruResponse);

    $xml->registerXPathNamespace('zs', 'http://www.loc.gov/zing/srw/');
    $xml->registerXPathNamespace('marc', 'http://www.loc.gov/MARC21/slim');
    $marcRecords = $xml->xpath('//zs:recordData/marc:record');

    if (count($marcRecords) != 1) {
        die("Could not determine unique record for PPN " . $ppn);
    }
    return $marcRecords[0];
}


function ExtractMetadataInformation($record) {
    $MARCToDCStylesheet = 'marcxml_to_dc.xslt';
    $xsl = new DOMDocument();
    $xsl->load($MARCToDCStylesheet);
    $xsltProcessor = new XSLTProcessor();
    $xsltProcessor->importStylesheet($xsl);

    $dc_json = $xsltProcessor->transformToXML($record);
    return json_decode($dc_json);
}


function FlattenArrayWithDuplicates($data) {
    return array_reduce($data, function ($carry, $entry) {
        foreach ($entry as $key => $value) {
            if (isset($carry[$key])) {
                $carry[$key] = (array) $carry[$key];
                $carry[$key][] = $value;
            } else {
                $carry[$key] = $value;
            }
        }
        return $carry;
    }, []);
}


function ConvertToDSpaceStructure($dc_metadata) {
    $dc_metadata = FlattenArrayWithDuplicates($dc_metadata);
    $title = $dc_metadata["dc.title"];
    $dspace_metadata_structure = [];
    foreach ($dc_metadata as $key => $value) {
        array_push($dspace_metadata_structure, [ 'key' => $key, 'value' => $value]);
    }
    return [ 'name' => $title, 'metadata' => $dspace_metadata_structure ];
}


function GenerateDSpaceMetadata($ppn, $metadata) {
    $dc_metadata = [];
    foreach($metadata as $key => $value) {
         if ($key == 'dc.contributor.author') {
             foreach ($value as $author_object) {
                 $author_object = json_decode(json_encode($author_object), true);
                 if (!array_key_exists("name", $author_object))
                     die("Missing author in name for ppn " . $ppn . ":\n" . print_r($metadata, true));
                 $name = $author_object["name"];
                 if (array_key_exists("role", $author_object)) {
                     $role = $author_object["role"];
                     switch($role) {
                         case "edt":
                             array_push($dc_metadata, [ 'dc.contributor.editor' => $name]);
                             break;
                         case "oth":
                             array_push($dc_metadata, [ 'dc.contributor.other' => $name]);
                             break;
                         default:
                             array_push($dc_metadata, [ 'dc.contributor.author' => $name]);
                      }
                 } else {
                      array_push($dc_metadata, [ 'dc.contributor.author' => $name]);
                 }
                 if (isset($author_object["gnd"]))
                     array_push($dc_metadata, [ 'utue.personen.pnd' => $name . '/' . $author_object["gnd"] ]);
             }
             continue;
         }

         if ($key == 'dc.language.iso') {
             $value = Languages::getAlpha2Code($value);
         }

         array_push($dc_metadata, [ $key => $value ]);
    }

    array_push($dc_metadata, [ 'utue.artikel.ppn' => $ppn ]);
    return ConvertToDSpaceStructure($dc_metadata);
}


function GetMetadataForPPN($ppn) {
     $k10plus_metadata = ExtractMetadataInformation(GetMetadataFromK10Plus($ppn));
     $dc_metadata = GenerateDSpaceMetadata($ppn, $k10plus_metadata);
     return json_encode($dc_metadata);
}


function CreateItem($config, $sessionToken, $ppn) {
    $dspaceApiUrl = rtrim($config['dspace_api_url'], '/');
    $dspaceCollectionUUID = $config['dspace_collection_uuid'];
    $collectionId = $dspaceCollectionUUID;
    $createItemUrl = $dspaceApiUrl . "/collections/$collectionId/items";
    $ch = curl_init($createItemUrl);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, [
        "Cookie: JSESSIONID=$sessionToken",
        "Content-Type: application/json",
        "Accept: application/json"
    ]);
    curl_setopt($ch, CURLOPT_POSTFIELDS, GetMetadataForPPN($ppn));

    $response = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    if ($httpCode !== 200 && $httpCode !== 201) {
        die("Failed to create item. HTTP Code: $httpCode\n");
    }

    $item = json_decode($response, true);
    if (json_last_error() !== JSON_ERROR_NONE) {
        die("Failed to parse JSON response.\n");
    }

    $uuid = $item['uuid'];
    if (empty(trim($uuid))) {
        die("Empty uuid\n");
    }
    echo "Item created successfully. Item UUID: $uuid\n";
    return $uuid;
}


function UploadPDFFile($config, $sessionToken, $uuid, $pdfFilePath) {
    $dspaceApiUrl = rtrim($config['dspace_api_url'], '/');

    if (!file_exists($pdfFilePath)) {
        die("PDF file '$pdfFilePath' not found.\n");
    }

    // Upload the PDF file as a bitstream
    $bitstreamUrl = $dspaceApiUrl . "/items/$uuid/bitstreams?name=" . urlencode(basename($pdfFilePath));
    $ch = curl_init($bitstreamUrl);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, [
        "Cookie: JSESSIONID=$sessionToken",
        "Content-Type: multipart/form-data",

    ]);
    curl_setopt($ch, CURLOPT_POSTFIELDS, [
        'file' => new CURLFile($pdfFilePath, mime_content_type($pdfFilePath), basename($pdfFilePath)),
        'name' =>  basename($pdfFilePath)
    ]);

    $response = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    if ($httpCode !== 200 && $httpCode !== 201) {
        die("Failed to upload PDF file. HTTP Code: $httpCode\n");
    }

    echo "PDF file uploaded successfully.\n";
}


function CloseSession($config, $sessionToken) {
    $dspaceApiUrl = rtrim($config['dspace_api_url'], '/');
    $logoutUrl = $dspaceApiUrl . '/logout';
    $ch = curl_init($logoutUrl);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, [
        "Cookie: JSESSIONID=$sessionToken",

    ]);
    $response = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    if ($httpCode != 200) {
        echo "Failed to close session\n";
    }
}


function ReadPPNAndPDFFileList($mapFilename) {
    if (!file_exists($mapFilename) || !is_readable($mapFilename)) {
        die("Unable to open \"" . $mapFilename . "\"");
    }

    $fileHandle = fopen($mapFilename, "r");
    if (!$fileHandle) {
        die("Error: Unable to open the file \"" . $mapFilename . "\"");
    }

    $ppns_to_filenames = [];

    while (($line = fgets($fileHandle)) !== false) {
        $line = trim($line);
        $columns = explode("|", $line);

        if (count($columns) < 2) {
            echo "Skipping invalid line: $line\n";
            continue;
        }
        $ppn = $columns[0];
        $pdfFilePath = $columns[1];
        $ppns_to_filenames[$ppn] = $pdfFilePath;
    }
    return $ppns_to_filenames;
}


function OpenUploadedPPNsFile($logFilename) {
    $fileHandle = fopen($logFilename, "w");
    if (!$fileHandle) {
        die("Error: Unable to open the file \"" . $logFilename . "\"");
    }
    return $fileHandle;
}


function WriteLogEntry($filehandle, $entry) {
    fwrite($filehandle, $entry);
    fflush($filehandle);
    echo $entry;
}


function CloseFile($fileHandle) {
    fclose($fileHandle);
}


function Main($argc, $argv) {
    if (php_sapi_name() !== 'cli') {
        die("This script must be run from the command line.\n");
    }

    if ($argc < 3) {
        die("Usage: php " . $argv[0] . " ppns_to_filenames uploaded_ppns.log\n");
    }

    $ppns_to_filenames_file = $argv[1];
    $uploaded_log_file = $argv[2];
    $config = GetConfig();
    $ppns_to_filenames = ReadPPNAndPDFFileList($ppns_to_filenames_file);
    $uploaded_log = OpenUploadedPPNsFile($uploaded_log_file);

    $sessionToken = GetSessionToken($config);
    foreach ($ppns_to_filenames as $ppn => $pdfFilePath) {
        GetMetadataForPPN($ppn);
        $uuid = CreateItem($config, $sessionToken, $ppn);
        UploadPDFFile($config, $sessionToken, $uuid, $pdfFilePath);

        WriteLogEntry($uploaded_log, $ppn . " => " . $pdfFilePath . "\n");
    }
    CloseSession($config, $sessionToken);
    CloseFile($uploaded_log);
}


Main($argc, $argv);
