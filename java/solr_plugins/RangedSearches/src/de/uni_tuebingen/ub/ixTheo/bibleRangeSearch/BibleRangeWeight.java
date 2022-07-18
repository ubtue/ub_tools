package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class BibleRangeWeight extends RangeWeight {
    private final static String FIELD = "bible_ranges";
    private final boolean isSearchingForBooks;

    public BibleRangeWeight(final BibleRangeQuery query, final BibleRange[] ranges, final Weight weight) {
        super(query, ranges, weight);

        boolean isSearchingForBooksLocal = false;
        for (final BibleRange range : ranges) {
            isSearchingForBooksLocal |= range.isEntireBook();
        }
        isSearchingForBooks = isSearchingForBooksLocal;
    }

    @Override
    protected boolean matches(final Document document) {
        final String dbField = document.get(FIELD);
        final BibleRange[] documentRanges = BibleRangeParser.getRangesFromDatabaseField(dbField);
        final BibleRange[] fieldRanges = isSearchingForBooks ? documentRanges : BibleRange.removeBooks(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }

    @Override
    protected float customScore(final Document doc) {
        final String dbField = doc.get(FIELD);
        if (dbField == null || dbField.isEmpty()) {
            return NOT_RELEVANT;
        }
        final Range[] field_ranges = BibleRangeParser.getRangesFromDatabaseField(dbField);
        return Range.getMatchingScore(field_ranges, ranges);
    }

    @Override
    public final boolean isCacheable(LeafReaderContext context) {
        return false;
    }
}
