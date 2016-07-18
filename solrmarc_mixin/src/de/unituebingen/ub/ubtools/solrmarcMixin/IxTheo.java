package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.*;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.Reader.*;
import java.io.*;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.VariableField;
import org.marc4j.marc.*;
import org.solrmarc.index.SolrIndexerMixin;
import org.solrmarc.tools.Utils;

public class IxTheo extends SolrIndexerMixin {
    private Set<String> ixTheoNotations = null;
    private static Set<String> unassigned = Collections.singleton("[Unassigned]");

    @Override
    public void perRecordInit(final Record record) {
        super.perRecordInit(record);
        ixTheoNotations = null;
    }

    /**
     * Split the colon-separated ixTheo notation codes into individual codes and
     * return them.
     */
    public Set<String> getIxTheoNotations(final Record record) {
        if (ixTheoNotations == null) {
            ixTheoNotations = new HashSet<>();
            final List fields = record.getVariableFields("652");
            if (fields.isEmpty()) {
                return ixTheoNotations;
            }
            // We should only have one 652 field
            final DataField data_field = (DataField) fields.iterator().next();
            // There should always be exactly one $a subfield
            final String contents = data_field.getSubfield('a').getData();
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
     * Overwrite the original VuFindIndexer getFormats to do away with the
     * single bucket approach, i.e. collect all formats you find, i.e. this is
     * the original code without premature returns which are left in commented
     * out
     *
     * @param record
     *            MARC record
     * @return set of record format
     */
    public Set<String> getMultipleFormats(final Record record) {
        Set<String> result = new LinkedHashSet<String>();
        String leader = record.getLeader().toString();
        ControlField fixedField = (ControlField) record.getVariableField("008");
        DataField title = (DataField) record.getVariableField("245");
        String formatString;
        char formatCode = ' ';
        char formatCode2 = ' ';
        char formatCode4 = ' ';

        // check if there's an h in the 245
        if (title != null) {
            if (title.getSubfield('h') != null) {
                if (title.getSubfield('h').getData().toLowerCase().contains("[electronic resource]")) {
                    result.add("Electronic");
                }
            }
        }

        // check the 007 - this is a repeating field
        List<VariableField> fields = record.getVariableFields("007");
        if (fields != null) {
            ControlField formatField;
            for (VariableField varField : fields) {
                formatField = (ControlField) varField;
                formatString = formatField.getData().toUpperCase();
                formatCode = formatString.length() > 0 ? formatString.charAt(0) : ' ';
                formatCode2 = formatString.length() > 1 ? formatString.charAt(1) : ' ';
                formatCode4 = formatString.length() > 4 ? formatString.charAt(4) : ' ';
                switch (formatCode) {
                case 'A':
                    switch (formatCode2) {
                    case 'D':
                        result.add("Atlas");
                        break;
                    default:
                        result.add("Map");
                        break;
                    }
                    break;
                case 'C':
                    switch (formatCode2) {
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
                    switch (formatCode2) {
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
                    switch (formatCode2) {
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
                    switch (formatCode2) {
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
                    switch (formatCode2) {
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
                    switch (formatCode2) {
                    case 'C':
                        result.add("VideoCartridge");
                        break;
                    case 'D':
                        switch (formatCode4) {
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
        }
        // check the Leader at position 6
        char leaderBit = leader.charAt(6);
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
        // if (!result.isEmpty()) {
        // return result;
        // }

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
     * @param record
     *            the record
     * @return format of record
     */
    public Set<String> getFormatsWithGermanHandling(final Record record) {
        // We've been facing the problem that the original SolrMarc cannot deal
        // with
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
     * Determine Record Format(s) including the electronic tag The electronic
     * category is filtered out in the actula getFormat function but needed to
     * determine the media type
     *
     * @param record
     *            the record
     * @return format of record
     */
    public Set getFormatIncludingElectronic(final Record record) {
        final Set<String> formats = new HashSet<>();
        Set<String> rawFormats = getFormatsWithGermanHandling(record);

        for (final String rawFormat : rawFormats) {
            if (rawFormat.equals("BookComponentPart") || rawFormat.equals("SerialComponentPart")) {
                formats.add("Article");
            } else {
                formats.add(rawFormat);
            }
        }

        // Determine whether an article is in fact a review
        final List<VariableField> _655Fields = record.getVariableFields("655");
        for (final VariableField _655Field : _655Fields) {
            final DataField dataField = (DataField) _655Field;
            if (dataField.getIndicator1() == ' ' && dataField.getIndicator2() == '7'
                    && dataField.getSubfield('a').getData().startsWith("Rezension")) {
                formats.remove("Article");
                formats.add("Review");
                break;
            }
        }

        // A review can also be indicated if 935$c set to "uwre"
        final List<VariableField> _935Fields = record.getVariableFields("935");
        for (final VariableField _935Field : _935Fields) {
            final DataField dataField = (DataField) _935Field;
            final Subfield cSubfield = dataField.getSubfield('c');
            if (cSubfield != null && cSubfield.getData().contains("uwre")) {
                formats.remove("Article");
                formats.add("Review");
                break;
            }
        }

        // Determine whether record is a 'Festschrift', i.e. has "fe" in 935$c
        for (final VariableField _935Field : _935Fields) {
            final DataField dataField = (DataField) _935Field;
            final Subfield cSubfield = dataField.getSubfield('c');
            if (cSubfield != null && cSubfield.getData().contains("fe")) {
                formats.add("Festschrift");
                break;
            }
        }

        // Rewrite all E-Books as electronic Books
        if (formats.contains("eBook")) {
            formats.remove("eBook");
            formats.add("Electronic");
            formats.add("Book");
        }

        return formats;
    }

    /**
     * Determine Format(s) but do away with the electronic tag
     *
     * @param record
     *            the record
     * @return mediatype of the record
     */

    public Set getFormat(final Record record) {
        Set<String> formats = getFormatIncludingElectronic(record);

        // Since we now have an additional facet mediatype we remove the
        // electronic label
        formats.remove("Electronic");

        return formats;
    }

    /**
     * Determine Mediatype For facets we need to differentiate between
     * electronic and non-electronic resources
     *
     * @param record
     *            the record
     * @return mediatype of the record
     */

    public Set getMediatype(final Record record) {
        final Set<String> mediatype = new HashSet<>();
        final Set<String> formats = getFormatIncludingElectronic(record);
        final String electronicRessource = "Electronic";
        final String nonElectronicRessource = "Non-Electronic";

        if (formats.contains(electronicRessource)) {
            mediatype.add(electronicRessource);
        } else {
            mediatype.add(nonElectronicRessource);
        }

        return mediatype;
    }

    /*
     * Get the appropriate translation map
     */

    Map<String, String> translation_map_en = new HashMap<String, String>();
    Map<String, String> translation_map_fr = new HashMap<String, String>();

    public Map<String, String> getTranslationMap(String langShortcut) throws IllegalArgumentException {

        Map<String, String> translation_map;

        switch (langShortcut) {
        case "en":
            translation_map = translation_map_en;
            break;
        case "fr":
            translation_map = translation_map_fr;
            break;
        default:
            throw new IllegalArgumentException("Invalid language shortcut: " + langShortcut);
        }

        // Only read the data from file if necessary
        if (translation_map.isEmpty()) {
            final String dir = "/usr/local/ub_tools/bsz_daten/";
            final String ext = "txt";
            final String basename = "normdata_translations";

            try {
                String translationsFilename = dir + basename + "_" + langShortcut + "." + ext;
                BufferedReader in = new BufferedReader(new FileReader(translationsFilename));
                String line;

                while ((line = in.readLine()) != null) {
                    String[] translations = line.split("\\|");
                    if (!translations[0].equals(""))
                        translation_map.put(translations[0], translations[1]);
                }
            } catch (IOException e) {
                System.err.println("Could not open file" + e.toString());
                System.exit(1);
            }
        }

        return translation_map;
    }

    /**
     * Translate set of terms to given language if a translation is found
     */

    public Set<String> translateTopics(Set<String> topics, String langShortcut) {
        if (langShortcut.equals("de"))
            return topics;

        Set<String> translated_topics = new HashSet<String>();
        Map<String, String> translation_map = getTranslationMap(langShortcut);

        for (String topic : topics) {
            topic = (translation_map.get(topic) != null) ? translation_map.get(topic) : topic;
            translated_topics.add(topic);
        }

        return translated_topics;
    }

    /**
     * Translate a single term to given language if a translation is found
     */

    public String translateTopic(String topic, String langShortcut) {
        if (langShortcut.equals("de"))
            return topic;

        Map<String, String> translation_map = getTranslationMap(langShortcut);
        return (translation_map.get(topic) != null) ? translation_map.get(topic) : topic;
    }

    /**
     * Determine Topics
     *
     * @param record
     *            the record
     * @return format of record
     */

    public Set<String> getTopics(final Record record, String fieldSpec, String separator, String langShortcut)
            throws FileNotFoundException {

        final Set<String> topics = new HashSet<String>();
        // It seems to be a general rule that in the fields that the $p fields
        // are converted to a '.'
        // $n is converted to a space if there is additional information
        Map<String, String> separators = parseTopicSeparators(separator);
        getTopicsCollector(record, fieldSpec, separators, topics);

        // Extract all translations that will be included
        return translateTopics(topics, langShortcut);
    }

    /**
     * Parse the field specifications
     */

    public Map<String, String> parseTopicSeparators(String separatorSpec) {

        final Map<String, String> separators = new HashMap<String, String>();

        // Split the string at unescaped ":"
        // See
        // http://stackoverflow.com/questions/18677762/handling-delimiter-with-escape-characters-in-java-string-split-method
        // (20160416)

        final String fieldDelim = ":";
        final String subfieldDelim = "$";
        final String esc = "\\";
        final String regexColon = "(?<!" + Pattern.quote(esc) + ")" + Pattern.quote(fieldDelim);
        String[] subfieldSeparatorList = separatorSpec.split(regexColon);
        for (String s : subfieldSeparatorList) {
            // System.out.println("separator Spec: " + s);
            // Create map of subfields and separators
            final String regexSubfield = "(?<!" + Pattern.quote(esc) + ")" + Pattern.quote(subfieldDelim)
                    + "([a-zA-Z])(.*)";
            Matcher subfieldMatcher = Pattern.compile(regexSubfield).matcher(s);

            // Extract the subfield
            if (subfieldMatcher.find()) {
                // Get $ and the character
                String subfield = subfieldMatcher.group(1);
                String separatorToUse = subfieldMatcher.group(2);
                // System.out.println("Inserting separators | subfield: " +
                // subfield + " - text: " + separatorToUse);
                separators.put(subfield, separatorToUse.replace(esc, ""));
            }
            // Use an expression that does not specify a subfield as default
            // value
            else {
                separators.put("default", s.replace(esc, ""));
            }
        }

        return separators;
    }

    /**
     * Generate Separator according to specification
     */

    public String getSubfieldBasedSeparator(Map<String, String> separators, char subfieldCodeChar) {

        String subfieldCodeString = Character.toString(subfieldCodeChar);
        String separator = separators.get(subfieldCodeString) != null ? separators.get(subfieldCodeString)
                : separators.get("default");

        return separator;

    }

    /**
     * Abstraction for iterating over the subfields
     */

    /**
     * Generic function for topics that abstracts from a set or lsit collector
     * It is based on original SolrIndex.getAllSubfieldsCollector but allows to
     * specify several different separators to concatenate the single subfields
     * Moreover Numeric subfields are filtered our since the do not contain data
     * to be displayed. Separators can be defined on a subfield basis as list in
     * the format separator_spec :== separator | subfield_separator_list
     * subfield_separator_list :== subfield_separator_spec |
     * subfield_separator_spec ":" subfield_separator_list |
     * subfield_separator_spec ":" separator subfield_separator_spec :==
     * subfield_spec separator subfield_spec :== "$" character_subfield
     * character_subfield :== A character subfield (e.g. p,n,t,x...) separator
     * :== separator_without_control_characters+ | separator "\:" separator |
     * separator "\$" separator separator_without_control_characters := All
     * characters without ":" and "$" | empty_string
     */

    public void getTopicsCollector(final Record record, String fieldSpec, Map<String, String> separators,
            Collection<String> collector) {

        String[] fldTags = fieldSpec.split(":");
        String fldTag;
        String subfldTags;
        List<VariableField> marcFieldList;

        for (int i = 0; i < fldTags.length; i++) {
            // Check to ensure tag length is at least 3 characters
            if (fldTags[i].length() < 3) {
                continue;
            }

            // Handle "Lokaldaten" appropriately
            if (fldTags[i].substring(0, 3).equals("LOK")) {

                if (fldTags[i].substring(3, 6).length() < 3) {
                    System.err.println("Invalid tag for \"Lokaldaten\": " + fldTags[i]);
                    continue;
                }
                // Save LOK-Subfield
                // Currently we do not support specifying an indicator
                fldTag = fldTags[i].substring(0, 6);
                subfldTags = fldTags[i].substring(6);
            } else {
                fldTag = fldTags[i].substring(0, 3);
                subfldTags = fldTags[i].substring(3);
            }

            // Case 1: We have a LOK-Field
            if (fldTag.startsWith("LOK")) {
                // Get subfield 0 since the "subtag" is saved here
                marcFieldList = record.getVariableFields("LOK");
                if (!marcFieldList.isEmpty()) {
                    for (VariableField vf : marcFieldList) {
                        DataField marcField = (DataField) vf;
                        StringBuffer buffer = new StringBuffer("");
                        Subfield subfield0 = marcField.getSubfield('0');
                        if (subfield0 == null || !subfield0.getData().startsWith(fldTag.substring(3, 6))) {
                            continue;
                        }
                        // Iterate over all given subfield codes
                        Pattern subfieldPattern = Pattern.compile(subfldTags.length() == 0 ? "." : subfldTags);
                        List<Subfield> subfields = marcField.getSubfields();
                        // Case 1: The separator specification is empty thus we add the subfields individually
                        if (separators.get("default").equals("")) {
                            for (Subfield subfield : subfields) {
                                 if (Character.isDigit(subfield.getCode()))
                                     continue;
                                 String term = subfield.getData().trim();
                                 if (term.length() > 0)
                                     collector.add(Utils.cleanData(term));
                            }
                        }
                        // Case 2: Generate a complex string using the separators 
                        else {
                            for (Subfield subfield : subfields) {
                                // Skip numeric fields
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                Matcher matcher = subfieldPattern.matcher("" + subfield.getCode());
                                if (matcher.matches()) {
                                    if (buffer.length() > 0) {
                                        String separator = getSubfieldBasedSeparator(separators, subfield.getCode());
                                        if (separator != null) {
                                            buffer.append(separator);
                                        }
                                    }
                                    buffer.append(subfield.getData().trim());
                                }
                            }
                            if (buffer.length() > 0)
                                collector.add(Utils.cleanData(buffer.toString()));
                        }
                    }
                }
            }

            // Case 2: We have an ordinary MARC field
            else {
                marcFieldList = record.getVariableFields(fldTag);
                if (!marcFieldList.isEmpty()) {
                    Pattern subfieldPattern = Pattern.compile(subfldTags.length() == 0 ? "." : subfldTags);
                    for (VariableField vf : marcFieldList) {
                        DataField marcField = (DataField) vf;
                        StringBuffer buffer = new StringBuffer("");
                        List<Subfield> subfields = marcField.getSubfields();
                        // Case 1: The separator specification is empty thus we add the subfields individually
                        if (separators.get("default").equals("")) { 
                            for (Subfield subfield : subfields) {
                                 if (Character.isDigit(subfield.getCode()))
                                     continue;
                                 String term = subfield.getData().trim();
                                 if (term.length() > 0)
                                     collector.add(Utils.cleanData(term));
                            }
                        }
                        // Case 2: Generate a complex string using the separators 
                        else {
                            for (Subfield subfield : subfields) {
                                // Skip numeric fields
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                Matcher matcher = subfieldPattern.matcher("" + subfield.getCode());
                                if (matcher.matches()) {
                                    if (buffer.length() > 0) {
                                        String separator = getSubfieldBasedSeparator(separators, subfield.getCode());
                                        if (separator != null) {
                                            buffer.append(separator);
                                        }
                                    }
                                    buffer.append(subfield.getData().trim());
                                }
                            }
                            if (buffer.length() > 0)
                                collector.add(Utils.cleanData(buffer.toString()));
                        }
                    }
                }
            }
        }

        return;
    }
}
