package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.Reader.*;
import java.io.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.*;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.VariableField;
import org.marc4j.marc.*;
import org.solrmarc.index.SolrIndexerMixin;
import org.solrmarc.tools.DataUtil;
import org.solrmarc.tools.Utils;
import de.unituebingen.ub.ubtools.solrmarcMixin.*;

public class IxTheo extends SolrIndexerMixin {
    private Set<String> ixTheoNotations = null;

    private final static Pattern NUMBER_END_PATTERN = Pattern.compile("([^\\d\\s<>]+)(\\s*<?\\d+(-\\d+)>?$)");


    /**
     * Split the colon-separated ixTheo notation codes into individual codes and
     * return them.
     */
    public Set<String> getIxTheoNotations(final Record record) {
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
        return ixTheoNotations;
    }

    public Set<String> getIxTheoNotationFacets(final Record record) {
        final Set<String> ixTheoNotations = getIxTheoNotations(record);
        if (ixTheoNotations.isEmpty()) {
            return TuelibMixin.UNASSIGNED_SET;
        }
        return ixTheoNotations;
    }


    public Set<String> getJournalIssue(final Record record) {
        final DataField _773Field = (DataField)record.getVariableField("773");
        if (_773Field == null)
            return null;

        final Subfield aSubfield = _773Field.getSubfield('a');
        if (aSubfield == null)
            return null;

        final Set<String> subfields = new LinkedHashSet<String>();
        subfields.add(aSubfield.getData());

        final Subfield gSubfield = _773Field.getSubfield('g');
        if (gSubfield != null)
            subfields.add(gSubfield.getData());

        final List<Subfield> wSubfields = _773Field.getSubfields('w');
        for (final Subfield wSubfield : wSubfields) {
            final String subfieldContents = wSubfield.getData();
            if (subfieldContents.startsWith("(DE-576)"))
                subfields.add(subfieldContents);
        }

        return subfields;
    }

    /**
     * Translate set of terms to given language if a translation is found
     */

    public Set<String> translateTopics(Set<String> topics, String langShortcut) {
        if (langShortcut.equals("de"))
            return topics;
        Set<String> translated_topics = new HashSet<String>();
        Map<String, String> translation_map = TuelibMixin.getTranslationMap(langShortcut);

        for (String topic : topics) {
            // Some ordinary topics contain words with an escaped slash as a
            // separator
            // See whether we can translate the single parts
            if (topic.contains("\\/")) {
                String[] subtopics = topic.split("\\/");
                int i = 0;
                for (String subtopic : subtopics) {
                    subtopics[i] = (translation_map.get(subtopic) != null) ? translation_map.get(subtopic) : subtopic;
                    ++i;
                }
                translated_topics.add(Utils.join(new HashSet<String>(Arrays.asList(subtopics)), "\\/"));

            } else {
                topic = (translation_map.get(topic) != null) ? translation_map.get(topic) : topic;
                translated_topics.add(topic);
            }
        }

        return translated_topics;
    }

    /**
     * Translate a single term to given language if a translation is found
     */

    public String translateTopic(String topic, String langShortcut) {
        if (langShortcut.equals("de"))
            return topic;

        Map<String, String> translation_map = TuelibMixin.getTranslationMap(langShortcut);
        Matcher numberEndMatcher = NUMBER_END_PATTERN.matcher(topic);

        // Some terms contain slash separated subterms, see whether we can
        // translate them
        if (topic.contains("\\/")) {
            String[] subtopics = topic.split("\\\\/");
            int i = 0;
            for (String subtopic : subtopics) {
                subtopics[i] = (translation_map.get(subtopic) != null) ? translation_map.get(subtopic) : subtopic;
                ++i;
            }
            topic = Utils.join(new HashSet<String>(Arrays.asList(subtopics)), "/");
        }
        // If we have a topic and a following number, try to separate the word and join it afterwards
        // This is especially important for time informations where we provide special treatment
        else if (numberEndMatcher.find()) {
            String topicText = numberEndMatcher.group(1);
            String numberExtension = numberEndMatcher.group(2);
            if (topicText.equals("Geschichte")) {
                switch (langShortcut) {
                case "en":
                    topic = "History" + numberExtension;
                    break;
                case "fr":
                    topic = "Histoire" + numberExtension;
                    break;
                }
            } else {
                topic = translation_map.get(topicText) != null ? translation_map.get(topicText) + numberExtension : topic;
            }
        } else {
            topic = (translation_map.get(topic) != null) ? translation_map.get(topic) : topic;
        }
        return topic;
    }

    /**
     * Determine Topics
     *
     * @param record
     *            the record
     * @return format of record
     */

    public Set<String> getTopics(final Record record, String fieldSpec, String separator, String langShortcut) throws FileNotFoundException {

        final Set<String> topics = new HashSet<String>();
        // It seems to be a general rule that in the fields that the $p fields
        // are converted to a '.'
        // $n is converted to a space if there is additional information
        Map<String, String> separators = parseTopicSeparators(separator);
        getTopicsCollector(record, fieldSpec, separators, topics, langShortcut);
        return topics;
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
        final String regexSubfield = "(?<!" + Pattern.quote(esc) + ")" + Pattern.quote(subfieldDelim) + "([a-zA-Z])(.*)";
        final Pattern SUBFIELD_PATTERN = Pattern.compile(regexSubfield);

        String[] subfieldSeparatorList = separatorSpec.split(regexColon);
        for (String s : subfieldSeparatorList) {
            // System.out.println("separator Spec: " + s);
            // Create map of subfields and separators

            Matcher subfieldMatcher = SUBFIELD_PATTERN.matcher(s);

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
        String separator = separators.get(subfieldCodeString) != null ? separators.get(subfieldCodeString) : separators.get("default");

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

    public void getTopicsCollector(final Record record, String fieldSpec, Map<String, String> separators, Collection<String> collector, String langShortcut) {

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
                        // Case 1: The separator specification is empty thus we
                        // add the subfields individually
                        if (separators.get("default").equals("")) {
                            for (Subfield subfield : subfields) {
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                String term = subfield.getData().trim();
                                if (term.length() > 0)
                                    // Escape slashes in single topics since
                                    // they interfere with KWCs
                                    collector.add(translateTopic(DataUtil.cleanData(term.replace("/", "\\/")), langShortcut));
                            }
                        }
                        // Case 2: Generate a complex string using the
                        // separators
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
                                    String term = subfield.getData().trim();
                                    buffer.append(translateTopic(term.replace("/", "\\/"), langShortcut));
                                }
                            }
                            if (buffer.length() > 0)
                                collector.add(DataUtil.cleanData(buffer.toString()));
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
                        // Case 1: The separator specification is empty thus we
                        // add the subfields individually
                        if (separators.get("default").equals("")) {
                            for (Subfield subfield : subfields) {
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                String term = subfield.getData().trim();
                                if (term.length() > 0)
                                    collector.add(translateTopic(DataUtil.cleanData(term.replace("/", "\\/")), langShortcut));
                            }
                        }
                        // Case 2: Generate a complex string using the
                        // separators
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
                                    String term = subfield.getData().trim();
                                    buffer.append(translateTopic(term.replace("/", "\\/"), langShortcut));
                                }
                            }
                            if (buffer.length() > 0)
                                collector.add(DataUtil.cleanData(buffer.toString()));
                        }
                    }
                }
            }
        }

        return;
    }
}
