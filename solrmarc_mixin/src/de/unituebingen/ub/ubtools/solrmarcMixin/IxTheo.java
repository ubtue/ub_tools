package de.unituebingen.ub.ubtools.solrmarcMixin;


import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.solrmarc.index.SolrIndexerMixin;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;


public class IxTheo extends SolrIndexerMixin {
    /**
     * Split the colon-separated ixTheo notation codes into individual codes and return them.
     */
    public Set<String> getIxTheoNotations(final Record record) {
        final List fields = record.getVariableFields("652");
        if (fields.isEmpty())
            return null;
        final DataField data_field = (DataField) fields.iterator().next(); // We should only have one 652 field.
        final String contents = data_field.getSubfield('a').getData(); // There should always be exactly one $a subfield.
        final String[] parts = contents.split(":");
        return new HashSet<>(Arrays.asList(parts));
    }

    /**
     * Determine Record Format(s)
     *
     * @param record the record
     * @return format of record
     */
    public Set getFormat(final Record record) {
        final Set<String> formats = new HashSet<>();
        final Set<String> rawFormats = indexer.getFormat(record);
        for (final String rawFormat : rawFormats) {
            if (rawFormat.equals("BookComponentPart") || rawFormat.equals("SerialComponentPart")) {
                formats.add("Article");
            } else {
                formats.add(rawFormat);
            }
        }
        return formats;
    }
}
