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
        if (fields != null) {
            // TODO: update loop to for(:) syntax, but problem with type casting.
            ControlField formatField;
            for (VariableField varField : fields) {
                formatField = (ControlField) varField;
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

    /**
     * Determine Topics
     *
     * @param record the record
     * @return format of record
     */

    //public Set<String> getTopics(final Record record, String fieldSpec, String[] separators) {
    public Set<String> getTopics(final Record record, String fieldSpec, String separator) {

       final Set<String> topics = new LinkedHashSet<String>();
       // It seems to be a general rule that in the fields that the $p fields are converted to a '.'
       // $n is converted to a space if there is additional information

       //String[] separators = {". ", " "};
       //String[] separators = {"|", "||", "|||", "||||", "|||||", "|||||||"};
       //String sspec = "\\::||:|||:||||";
       //String sspec = "$p \\$xxxx :$t&&:$x&&&:$v&&&&";
       Map<String, String> separators = parseTopicSeparators(separator);

       //System.out.println("Specs: ");
       //for (Map.Entry<String, String> entry : separators.entrySet()) {
       //     System.out.print(entry.getKey() + " is mapped to "  + entry.getValue() + "-----");
       //}

       getTopicsCollector(record, fieldSpec, separators, topics);

       return topics;

    }

   /**
    * Parse the field specifications
    */
   
    public Map<String,String> parseTopicSeparators(String separatorSpec){
  
       final Map<String,String> separators = new LinkedHashMap<String,String>();
       
       // Split the string at unescaped ":"
       // See http://stackoverflow.com/questions/18677762/handling-delimiter-with-escape-characters-in-java-string-split-method (20160416)
       
       final String fieldDelim = ":";
       final String subfieldDelim = "$";
       final String esc = "\\";
       final String regexColon = "(?<!" + Pattern.quote(esc) + ")" + Pattern.quote(fieldDelim);
       
       String[] subfieldSeparatorList = separatorSpec.split(regexColon);
       for(String s : subfieldSeparatorList) {
           // System.out.println("separator Spec: " + s);
           // Create map of subfields and separators
           final String regexSubfield = "(?<!" + Pattern.quote(esc) + ")" +  Pattern.quote(subfieldDelim) + "([a-zA-Z])(.*)";
           Matcher subfieldMatcher = Pattern.compile(regexSubfield).matcher(s);
           
           // Extract the subfield
           if(subfieldMatcher.find()) {
               // Get $ and the character
               String subfield = subfieldMatcher.group(1);
               String separatorToUse = subfieldMatcher.group(2);
               //System.out.println("Inserting separators | subfield: " + subfield + " - text: " + separatorToUse);
               separators.put(subfield, separatorToUse.replace(esc, ""));
           }
           // Use an expression that does not specify a subfield as default value
           else{
              separators.put("default", s.replace(esc, ""));
           }

       }
        
       return separators;

    }
    

   /**
    * Generic function for topics that abstracts from a set or lsit collector
    * It is based on original SolrIndex.getAllSubfieldsCollector but allows
    * to specify several different separators to concatenate the single subfields
    * Moreover Numeric subfields are filtered our since the do not contain data
    * to be displayed. 
    * Separators can be defined on a subfield basis as list in the format
    * separator_spec :== separator | subfield_separator_list
    * subfield_separator_list :== subfield_separator_spec | 
    *                             subfield_separator_spec ":" subfield_separator_list | 
    *                             subfield_separator_spec ":" separator
    * subfield_separator_spec :== subfield_spec separator 
    * subfield_spec :== "$" character_subfield
    * character_subfield :== A character subfield (e.g. p,n,t,x...)
    * separator :== separator_without_control_characters+ | 
                    separator "\:" separator | separator "\$" separator
    * separator_without_control_characters := All characters without ":" and "$" | empty_string
    */

    public void getTopicsCollector(final Record record, String fieldSpec,
                                          Map<String,String> separators, Collection<String> collector) {

        String[] fldTags = fieldSpec.split(":");

        for (int i = 0; i < fldTags.length; i++)
        {
            // Check to ensure tag length is at least 3 characters
            if (fldTags[i].length() < 3)
            {
                System.err.println("Invalid tag specified: " + fldTags[i]);
                continue;
            }

            String fldTag = fldTags[i].substring(0, 3);

            String subfldTags = fldTags[i].substring(3);

            List<VariableField> marcFieldList = record.getVariableFields(fldTag);
            if (!marcFieldList.isEmpty())
            {
                Pattern subfieldPattern = Pattern.compile(subfldTags.length() == 0 ? "." : subfldTags);
                for (VariableField vf : marcFieldList)
                {
                    int separator_index = 0;
                    DataField marcField = (DataField) vf;
                    StringBuffer buffer = new StringBuffer("");
                    List<Subfield> subfields = marcField.getSubfields();
                    for (Subfield subfield : subfields)
                    {
                        // Skip numeric fields
                        if (Character.isDigit(subfield.getCode()))
                            continue;
                        Matcher matcher = subfieldPattern.matcher("" + subfield.getCode());
                        if (matcher.matches())
                        {
                            if (buffer.length() > 0) {
                                String subfieldCode = Character.toString(subfield.getCode());
                                String separator = separators.get(subfieldCode) != null ? 
                                                     separators.get(subfieldCode) : separators.get("default");
                                buffer.append(separator);
                            }
                            buffer.append(subfield.getData().trim());
                        }
                    }
                    if (buffer.length() > 0)
                        collector.add(Utils.cleanData(buffer.toString()));
                }
            }
        }

        return;
   }

}
