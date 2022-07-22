package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class TimeAspectRangeWeight extends RangeWeight {
    public TimeAspectRangeWeight(final TimeAspectRangeQuery query, final TimeAspectRange[] ranges, final Weight weight) {
        super(query, ranges, weight);
    }

    @Override
    protected String getRangeFieldName() {
        return "time_aspect_ranges";
    }

    @Override
    protected Range[] getRangesFromDatabaseField(final String dbField) {
        return TimeAspectRangeParser.getRangesFromDatabaseField(dbField);
    }
}
