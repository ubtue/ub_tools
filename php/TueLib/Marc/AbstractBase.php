<?php

/**
 * Abstract class for all kinds of Marc classes
 *
 * All Marc classes need to be at least Importable/Exportable from/to XML
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Marc;
abstract class AbstractBase {
    abstract static public function ImportFromDomElement(\DOMElement $element);
    abstract public function ExportToXmlString();

    public function ExportToDomElement() {
        $xml_string = '<?xml version="1.0"?>' . PHP_EOL;
        $xml_string .= '<marc:collection xmlns:marc="http://www.loc.gov/MARC21/slim" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd">';
        $xml_string .= $this->ExportToXmlString();
        $xml_string .= '</marc:collection>';

        $dom = new \DOMDocument();
        $dom->loadXML($xml_string);
        return $dom->documentElement->firstChild;
    }

    public function __toString() {
        return $this->ExportToXmlString();
    }
}

?>