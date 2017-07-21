<?php

/**
 * This class represents a whole record.
 * It can contain a leader and multiple control fields
 * and data fields with subfields.
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Marc;
class Record extends AbstractBase {

    protected $_leader;
    protected $_controlFields   = [];
    protected $_dataFields      = [];

    static public function ImportFromDomElement(\DOMElement $element) {
        $record = new Record();

        $record->controlFields = [];
        $record->dataFields = [];
        foreach ($element->childNodes as $child) {
            if ($child instanceof \DOMElement) {
                switch ($child->tagName) {
                    case 'marc:leader':
                        $record->SetLeader(Leader::ImportFromDomElement($child));
                        break;
                    case 'marc:controlfield':
                        $record->AddControlField(ControlField::ImportFromDomElement($child));
                        break;
                    case 'marc:datafield':
                        $record->AddDataField(DataField::ImportFromDomElement($child));
                        break;
                }
            }
        }

        return $record;
    }

    public function ExportToXmlString() {
        $string = '<marc:record>' . PHP_EOL;
        $string .= $this->_leader->ExportToXmlString() . PHP_EOL;
        foreach ($this->_controlFields as $controlField) {
            $string .= $controlField->ExportToXmlString() . PHP_EOL;
        }
        foreach ($this->_dataFields as $dataField) {
            $string .= $dataField->ExportToXmlString() . PHP_EOL;
        }

        $string .= '</marc:record>' . PHP_EOL;
        return $string;
    }

    /* Data functions & helpers */
    public function AddControlField(ControlField $field) {
        $this->_controlFields[] = $field;
    }

    public function AddDataField(DataField $field) {
        $this->_dataFields[] = $field;
    }

    public function GetControlField($tag) {
        foreach ($this->_controlFields as $field) {
            if ($field->GetTag() == $tag) return $field;
        }

        throw new \Exception('control field not found: ' . $tag);
    }

    public function GetControlFields($tag) {
        return $this->_controlFields;
    }

    public function GetDataFields($tag=null) {
        $fields = [];
        foreach ($this->_dataFields as $field) {
            if ($tag == null || $tag == $field->tag) {
                $fields[] = $field;
            }
        }
        return $fields;
    }

    public function GetLeader() {
        return $this->_leader;
    }

    public function GetPPN() {
        $field = $this->GetControlField('001');
        return $field->GetValue();
    }

    public function SetLeader(Leader $leader) {
        $this->_leader = $leader;
    }
}

?>