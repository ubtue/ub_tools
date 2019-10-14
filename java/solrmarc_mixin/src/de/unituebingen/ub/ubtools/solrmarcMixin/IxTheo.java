package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.Reader.*;
import java.util.logging.Logger;
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
    protected static Logger logger = Logger.getLogger(IxTheo.class.getName());


    static boolean parseIniFileLine(final String line, final StringBuilder key, final StringBuilder value)
    {
        final int firstEqualPos = line.indexOf('=');
        if (firstEqualPos == -1)
            return false;

        key.append(line.substring(0, firstEqualPos - 1).trim());

        final String possiblyQuotedValue = line.substring(firstEqualPos + 1).trim();
        if (possiblyQuotedValue.length() < 2 || possiblyQuotedValue.charAt(0) != '"'
            || possiblyQuotedValue.charAt(possiblyQuotedValue.length() - 1) != '"')
            return false;

        value.append(possiblyQuotedValue.substring(1, possiblyQuotedValue.length() - 2));

        return true;
    }

    static void processLanguageIniFile(final File iniFile, final HashMap<String, TreeSet<String>> ixtheoNotationsToDescriptionsMap,
                                       final String entryPrefix)
    {
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new FileReader(iniFile));
        } catch (final FileNotFoundException ex) {
            logger.severe("can't create a BufferedReader for \"" + iniFile.getName() + "\"!");
            System.exit(1);
        }

        try {
            final int ENTRY_PREFIX_LENGTH = entryPrefix.length();
            String line;
            while ((line = reader.readLine()) != null) {
                if (!line.startsWith(entryPrefix))
                    continue;

                final StringBuilder key = new StringBuilder();
                final StringBuilder value = new StringBuilder();
                if (!parseIniFileLine(line, key, value) || value.length() < key.length() + 2 /* 1 space and at least one character */)
                    continue;

                final String notationCode = key.toString().substring(ENTRY_PREFIX_LENGTH);
                final String notationDescription = value.toString().substring(key.length() - ENTRY_PREFIX_LENGTH).trim();

                if (!ixtheoNotationsToDescriptionsMap.containsKey(notationCode)) {
                    final TreeSet<String> newSet = new TreeSet<String>();
                    newSet.add(notationDescription);
                    ixtheoNotationsToDescriptionsMap.put(notationCode, newSet);
                } else {
                    final TreeSet<String> set = ixtheoNotationsToDescriptionsMap.get(notationCode);
                    set.add(notationDescription);
                }
            }
        } catch (final IOException ex) {
            logger.severe("We should *never* get here!");
            System.exit(1);
        }
    }

    static HashMap<String, TreeSet<String>> processLanguageIniFiles()
    {
        final HashMap<String, TreeSet<String>> ixtheoNotationsToDescriptionsMap = new HashMap<>();

        final File[] dir_entries = new File("/usr/local/vufind/local/tuefind/languages").listFiles();
        for (final File dir_entry : dir_entries) {
            if (dir_entry.getName().length() != 6 || !dir_entry.getName().endsWith(".ini")) {
                logger.warning("Unexpected language file: " + dir_entry.getName());
                continue;
            }

            processLanguageIniFile(dir_entry, ixtheoNotationsToDescriptionsMap, "ixtheo-");
        }

        return ixtheoNotationsToDescriptionsMap;
    }

    private HashMap<String, TreeSet<String>> ixtheoNotationsToDescriptionsMap = processLanguageIniFiles();

    /**
     * Split the colon-separated ixTheo notation codes into individual codes and
     * return them.
     */
    public Set<String> getIxTheoNotations(final Record record) {
        final Set<String> ixTheoNotations = new TreeSet<>();
        final List fields = record.getVariableFields("652");
        if (fields.isEmpty())
            return ixTheoNotations;

        // We should only have one 652 field
        final DataField data_field = (DataField) fields.iterator().next();
        // There should always be exactly one $a subfield
        final String contents = data_field.getSubfield('a').getData();
        final String[] parts = contents.split(":");
        Collections.addAll(ixTheoNotations, parts);

        return ixTheoNotations;
    }

    /**
     * Split the colon-separated ixTheo notation codes into individual codes and
     * return them and the expanded and translated versions.
     */
    public Set<String> getExtendedIxTheoNotations(final Record record) {
        final Set<String> notationCodes = getIxTheoNotations(record);

        final HashSet<String> expendedIxTheoNotations = new HashSet<>();
        for (final String notationCode : getIxTheoNotations(record)) {
            final Set<String> notationDescriptions = ixtheoNotationsToDescriptionsMap.get(notationCode);
            if (notationDescriptions != null) {
                for (final String notationDescription : notationDescriptions)
                    expendedIxTheoNotations.add(notationDescription);
            }
            expendedIxTheoNotations.add(notationCode);
        }

        return expendedIxTheoNotations;
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

    public String getIsCanonLaw(final Record record) {
        return record.getVariableFields("CAN").isEmpty() ? "false" : "true";
    }
}
