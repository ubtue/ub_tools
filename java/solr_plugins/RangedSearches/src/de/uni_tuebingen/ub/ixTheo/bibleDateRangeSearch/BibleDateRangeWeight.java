package de.uni_tuebingen.ub.ixTheo.bibleDateRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class BibleDateRangeWeight extends RangeWeight {
    private final static float NOT_RELEVANT = Float.NEGATIVE_INFINITY;
    private final boolean isSearchingForBooks;

    public BibleDateRangeWeight(final BibleDateRangeQuery query, final BibleDateRange[] ranges, final Weight weight) {
        super(query, ranges, weight);

        boolean isSearchingForBooksLocal = false;
        for (final BibleDateRange range : ranges) {
            isSearchingForBooksLocal |= range.isEntireBook();
        }
        isSearchingForBooks = isSearchingForBooksLocal;
    }

    @Override
    protected String getRangeFieldName() {
        return "bible_ranges";
    }

    @Override
    protected Range[] getRangesFromDatabaseField(final String dbField) {
        return BibleDateRangeParser.getRangesFromDatabaseField(dbField);
    }

    @Override
    protected boolean matches(final Document document) {
        final String dbField = document.get(getRangeFieldName());
        final BibleDateRange[] documentRanges = BibleDateRangeParser.getRangesFromDatabaseField(dbField);
        final BibleDateRange[] fieldRanges = isSearchingForBooks ? documentRanges : BibleDateRange.removeBooks(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }
}
