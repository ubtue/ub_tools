<?php
/**
 * Example XML parser for Marc21
 *
 * Just use 'marc:record' in _callback_tagnames to get a callback with a
 * DOMElement for each marc:record.
 *
 * Do with it whatever you like.
 * E.g. read, manipulate, remove...
 * or use MarcRecord::InitWithDomElement(), manipulate it, and write it back
 * using MarcRecord->SaveToDomElement()
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Xml\Parser;
require_once('TueLib/Marc.php');
require_once('TueLib/Xml.php');
use TueLib\Marc\Record,
    TueLib\Marc\DataField,
    TueLib\Marc\SubField;
class ExampleMarcXml extends AbstractDom {

    /**
     * set callback to be called with marc:record elements only
     * @var array
     */
    protected $_callback_tagnames = ['marc:record'];

    /**
     * Testing can be done in this function
     * @param DOMElement $element
     * @return DOMElement
     */
    protected function _callbackDomElement(\DOMElement $element) {
        // Example 1: set Attribute
        //$element->setAttribute('parsed', 'true');

        // Example 2: print XML
        //print $element->ownerDocument->saveXML($element) . PHP_EOL;

        // Example 3: Use MarcRecord-class to get PPN
        $record = Record::ImportFromDomElement($element);
        $ppn    = $record->GetPPN();
        print 'PPN: ' . $ppn . PHP_EOL;

        // Example 4: Use MarcRecord-class to add field
        /*
        $record = Record::ImportFromDomElement($element);
        $dataField              = new DataField('LOK');
        $subField               = new SubField(0, 'test123');
        $dataField->AddSubField($subField);
        $record->AddDataField($dataField);
        return $record->ExportToDomElement();
        */

        // Example 5: Remove whole element
        //return null;
    }
}

$parser = new ExampleMarcXml();
$parser->parse('../bsz_daten/crossref_marc.xml');

?>