package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class TimeAspectRangeWeight extends RangeWeight {
    private final static String FIELD = "time_aspect_ranges";

    public TimeAspectRangeWeight(final TimeAspectRangeQuery query, final TimeAspectRange[] ranges, final Weight weight) {
        super(query, ranges, weight);
    }

    @Override
    protected boolean matches(final Document document) {
        final String dbField = document.get(FIELD);
        final TimeAspectRange[] documentRanges = TimeAspectRangeParser.getRangesFromDatabaseField(dbField);
        return documentRanges.length != 0 && Range.hasIntersections(ranges, documentRanges);
    }

    @Override
    protected float customScore(final Document doc) {
        final String dbField = doc.get(FIELD);
        if (dbField == null || dbField.isEmpty()) {
            return NOT_RELEVANT;
        }
        final Range[] field_ranges = TimeAspectRangeParser.getRangesFromDatabaseField(dbField);
        return Range.getMatchingScore(field_ranges, ranges);
    }

    @Override
    public final boolean isCacheable(LeafReaderContext context) {
        return false;
    }
}
