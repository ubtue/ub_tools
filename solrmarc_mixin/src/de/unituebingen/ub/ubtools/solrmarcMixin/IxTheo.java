package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.Reader.*;
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
}
