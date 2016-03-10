package de.unituebingen.ub.ubtools.solrmarcMixin;


import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.solrmarc.index.SolrIndexerMixin;

import java.util.*;


public class IxTheo extends SolrIndexerMixin {
    private Set<String> ixTheoNotations = null;

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
            return Collections.singleton("[Unassigned]");
        }
        return ixTheoNotations;
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
