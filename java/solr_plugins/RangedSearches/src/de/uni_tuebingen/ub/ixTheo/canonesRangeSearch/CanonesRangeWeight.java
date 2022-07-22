package de.uni_tuebingen.ub.ixTheo.canonesRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class CanonesRangeWeight extends RangeWeight {
    private final boolean isSearchingForCodices;

    public CanonesRangeWeight(final CanonesRangeQuery query, final CanonesRange[] ranges, final Weight weight) {
        super(query, ranges, weight);
        boolean isSearchingForCodicesLocal = false;
        for (final CanonesRange range : ranges) {
            isSearchingForCodicesLocal |= range.isEntireCodex();
        }
        isSearchingForCodices = isSearchingForCodicesLocal;
    }

    @Override
    protected String getRangeFieldName() {
        return "canon_law_ranges";
    }

    @Override
    protected Range[] getRangesFromDatabaseField(final String dbField) {
        return CanonesRangeParser.getRangesFromDatabaseField(dbField);
    }

    @Override
    protected boolean matches(final Document document) {
        final String dbField = document.get(getRangeFieldName());
        final CanonesRange[] documentRanges = CanonesRangeParser.getRangesFromDatabaseField(dbField);
        final CanonesRange[] fieldRanges = isSearchingForCodices ? documentRanges : CanonesRange.removeCodices(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }
}
