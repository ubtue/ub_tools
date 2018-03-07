package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.util.List;
import org.marc4j.marc.Record;
import org.marc4j.marc.VariableField;
import org.solrmarc.index.SolrIndexerMixin;

public class KrimDok extends SolrIndexerMixin {
    public String isAvailableForPDA(final Record record) {
        final List fields = record.getVariableFields("PDA");
        return Boolean.toString(!fields.isEmpty());
    }
}
