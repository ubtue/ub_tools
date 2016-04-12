package de.unituebingen.ub.ubtools.solrmarcMixin;


import org.marc4j.marc.DataField;
import org.marc4j.marc.VariableField;
import org.marc4j.marc.Record;
import org.solrmarc.index.SolrIndexerMixin;
import org.solrmarc.tools.Utils;
import org.marc4j.marc.*;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

import java.util.*;


public class IxTheo extends SolrIndexerMixin {
    private Set<String> ixTheoNotations = null;
    private static Set<String> unassigned = Collections.singleton("[Unassigned]");

    @Override
    public void perRecordInit(final Record record) {
        super.perRecordInit(record);
        ixTheoNotations = null;
    }

    /**
     * Split the colon-separated ixTheo notation codes into individual codes and return them.
     */
    public Set<String> getIxTheoNotations(final Record record) {
        if (ixTheoNotations == null) {
            ixTheoNotations = new HashSet<>();
            final List fields = record.getVariableFields("652");
            if (fields.isEmpty()) {
                return ixTheoNotations;
            }
            final DataField data_field = (DataField) fields.iterator().next(); // We should only have one 652 field.
            final String contents = data_field.getSubfield('a').getData(); // There should always be exactly one $a subfield.
            final String[] parts = contents.split(":");
            Collections.addAll(ixTheoNotations, parts);
        }
        return ixTheoNotations;
    }

    public Set<String> getIxTheoNotationFacets(final Record record) {
        final Set<String> ixTheoNotations = getIxTheoNotations(record);
        if (ixTheoNotations.isEmpty()) {
            return unassigned;
        }
        return ixTheoNotations;
    }

   /**
     * Determine Record Formats
     *
     * Overwrite the original VuFindIndexer getFormats to do 
     * away with the single bucket approach, i.e. collect all
     * formats you find, i.e. this is the original code 
     * without premature returns which are left in commented out
     *
     * @param  record MARC record
     * @return set of record format
     */ 

 public Set<String> getMultipleFormats(final Record record){
        Set<String> result = new LinkedHashSet<String>();
        String leader = record.getLeader().toString();
        char leaderBit;
        ControlField fixedField = (ControlField) record.getVariableField("008");
        DataField title = (DataField) record.getVariableField("245");
        String formatString;
        char formatCode = ' ';
        char formatCode2 = ' ';
        char formatCode4 = ' ';

        // check if there's an h in the 245
        if (title != null) {
            if (title.getSubfield('h') != null){
                if (title.getSubfield('h').getData().toLowerCase().contains("[electronic resource]")) {
                    result.add("Electronic");
                    //return result;
                }
            }
        }

        // check the 007 - this is a repeating field
        List<VariableField> fields = record.getVariableFields("007");
        Iterator<VariableField> fieldsIter = fields.iterator();
        if (fields != null) {
            // TODO: update loop to for(:) syntax, but problem with type casting.
            ControlField formatField;
            while(fieldsIter.hasNext()) {
                formatField = (ControlField) fieldsIter.next();
                formatString = formatField.getData().toUpperCase();
                formatCode = formatString.length() > 0 ? formatString.charAt(0) : ' ';
                formatCode2 = formatString.length() > 1 ? formatString.charAt(1) : ' ';
                formatCode4 = formatString.length() > 4 ? formatString.charAt(4) : ' ';
                switch (formatCode) {
                    case 'A':
                        switch(formatCode2) {
                            case 'D':
                                result.add("Atlas");
                                break;
                            default:
                                result.add("Map");
                                break;
                        }
                        break;
                    case 'C':
                        switch(formatCode2) {
                            case 'A':
                                result.add("TapeCartridge");
                                break;
                            case 'B':
                                result.add("ChipCartridge");
                                break;
                            case 'C':
                                result.add("DiscCartridge");
                                break;
                            case 'F':
                                result.add("TapeCassette");
                                break;
                            case 'H':
                                result.add("TapeReel");
                                break;
                            case 'J':
                                result.add("FloppyDisk");
                                break;
                            case 'M':
                            case 'O':
                                result.add("CDROM");
                                break;
                            case 'R':
                                // Do not return - this will cause anything with an
                                // 856 field to be labeled as "Electronic"
                                break;
                            default:
                                result.add("Software");
                                break;
                        }
                        break;
                    case 'D':
                        result.add("Globe");
                        break;
                    case 'F':
                        result.add("Braille");
                        break;
                    case 'G':
                        switch(formatCode2) {
                            case 'C':
                            case 'D':
                                result.add("Filmstrip");
                                break;
                            case 'T':
                                result.add("Transparency");
                                break;
                            default:
                                result.add("Slide");
                                break;
                        }
                        break;
                    case 'H':
                        result.add("Microfilm");
                        break;
                    case 'K':
                        switch(formatCode2) {
                            case 'C':
                                result.add("Collage");
                                break;
                            case 'D':
                                result.add("Drawing");
                                break;
                            case 'E':
                                result.add("Painting");
                                break;
                            case 'F':
                                result.add("Print");
                                break;
                            case 'G':
                                result.add("Photonegative");
                                break;
                            case 'J':
                                result.add("Print");
                                break;
                            case 'L':
                                result.add("Drawing");
                                break;
                            case 'O':
                                result.add("FlashCard");
                                break;
                            case 'N':
                                result.add("Chart");
                                break;
                            default:
                                result.add("Photo");
                                break;
                        }
                        break;
                    case 'M':
                        switch(formatCode2) {
                            case 'F':
                                result.add("VideoCassette");
                                break;
                            case 'R':
                                result.add("Filmstrip");
                                break;
                            default:
                                result.add("MotionPicture");
                                break;
                        }
                        break;
                    case 'O':
                        result.add("Kit");
                        break;
                    case 'Q':
                        result.add("MusicalScore");
                        break;
                    case 'R':
                        result.add("SensorImage");
                        break;
                    case 'S':
                        switch(formatCode2) {
                            case 'D':
                                result.add("SoundDisc");
                                break;
                            case 'S':
                                result.add("SoundCassette");
                                break;
                            default:
                                result.add("SoundRecording");
                                break;
                        }
                        break;
                    case 'V':
                        switch(formatCode2) {
                            case 'C':
                                result.add("VideoCartridge");
                                break;
                            case 'D':
                                switch(formatCode4) {
                                    case 'S':
                                        result.add("BRDisc");
                                        break;
                                    case 'V':
                                    default:
                                        result.add("VideoDisc");
                                        break;
                                }
                                break;
                            case 'F':
                                result.add("VideoCassette");
                                break;
                            case 'R':
                                result.add("VideoReel");
                                break;
                            default:
                                result.add("Video");
                                break;
                        }
                        break;
                }
            }
//          if (!result.isEmpty()) {
//              return result;
//          }
        }
        // check the Leader at position 6
        leaderBit = leader.charAt(6);
        switch (Character.toUpperCase(leaderBit)) {
            case 'C':
            case 'D':
                result.add("MusicalScore");
                break;
            case 'E':
            case 'F':
                result.add("Map");
                break;
            case 'G':
                result.add("Slide");
                break;
            case 'I':
                result.add("SoundRecording");
                break;
            case 'J':
                result.add("MusicRecording");
                break;
            case 'K':
                result.add("Photo");
                break;
            case 'M':
                result.add("Electronic");
                break;
            case 'O':
            case 'P':
                result.add("Kit");
                break;
            case 'R':
                result.add("PhysicalObject");
                break;
            case 'T':
                result.add("Manuscript");
                break;
        }
//      if (!result.isEmpty()) {
//            return result;
//        }

        // check the Leader at position 7
        leaderBit = leader.charAt(7);
        switch (Character.toUpperCase(leaderBit)) {
            // Monograph
            case 'M':
                if (formatCode == 'C') {
                    result.add("eBook");
                } else {
                    result.add("Book");
                }
                break;
            // Component parts
            case 'A':
                result.add("BookComponentPart");
                break;
            case 'B':
                result.add("SerialComponentPart");
                break;
            // Serial
            case 'S':
                // Look in 008 to determine what type of Continuing Resource
                formatCode = fixedField.getData().toUpperCase().charAt(21);
                switch (formatCode) {
                    case 'N':
                        result.add("Newspaper");
                        break;
                    case 'P':
                        result.add("Journal");
                        break;
                    default:
                        result.add("Serial");
                        break;
                }
        }

        // Nothing worked!
        if (result.isEmpty()) {
            result.add("Unknown");
        }

        return result;
    }




   /**
     * Determine Record Format(s)
     *
     * @param record the record
     * @return format of record
     */

    public Set<String> getFormatsWithGermanHandling(final Record record){

       // We've been facing the problem that the original SolrMarc cannot deal with 
       // german descriptions in the 245h and thus assigns a wrong format 
       // for e.g. electronic resource
       // Thus we must handle this manually

       Set<String> rawFormats = new LinkedHashSet<String>();
       DataField title = (DataField) record.getVariableField("245");

       if (title != null) {
           if (title.getSubfield('h') != null) {
                if (title.getSubfield('h').getData().toLowerCase().contains("[elektronische ressource]")) {
                   rawFormats.add("Electronic");
                   rawFormats.addAll(getMultipleFormats(record));
                   return rawFormats;
                } else {
                   return getMultipleFormats(record);
                }
           }
       }

       // Catch case of empty title
       return getMultipleFormats(record);

    }

    /**
     * Determine Record Format(s)
     *
     * @param record the record
     * @return format of record
     */
    public Set getFormat(final Record record) {
        final Set<String> formats = new HashSet<>();
        Set<String> rawFormats = getFormatsWithGermanHandling(record);
        
        for (final String rawFormat : rawFormats) {
            if (rawFormat.equals("BookComponentPart") || rawFormat.equals("SerialComponentPart")) {
                formats.add("Article");
            } else {
                formats.add(rawFormat);
            }
        }

        final List<VariableField> _655Fields = record.getVariableFields("655");
	for (final VariableField _655Field : _655Fields) {
	    final DataField dataField = (DataField)_655Field;
	    if (dataField.getIndicator1() == ' ' && dataField.getIndicator2() == '7'
		&& dataField.getSubfield('a').getData().startsWith("Rezension"))
            {
		formats.clear();
		formats.add("Review");
		break;
	    }
	}

        final List<VariableField> _935Fields = record.getVariableFields("935");
	for (final VariableField _935Field : _935Fields) {
	    final DataField dataField = (DataField)_935Field;
	    if (dataField.getIndicator1() == ' ' && dataField.getIndicator2() == '7'
		&& dataField.getSubfield('c').getData().equals("uwre"))
            {
		formats.clear();
		formats.add("Review");
		break;
	    }
	}

        return formats;
    }

}
