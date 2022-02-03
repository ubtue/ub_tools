package de.uni_tuebingen.ub.ixTheo.timeAspectDateRangeSearch;


import java.io.IOException;
import java.util.Arrays;

import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.ScoreMode;
import org.apache.lucene.search.Weight;

import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeQuery;


class TimeAspectDateRangeQuery extends RangeQuery {
    TimeAspectDateRangeQuery(final Query query, final TimeAspectDateRange[] ranges) {
        super(query, Arrays.copyOf(ranges, ranges.length, Range[].class));
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final ScoreMode scoreMode, final float boost) throws IOException {
        return new TimeAspectDateRangeWeight(this, Arrays.copyOf(ranges, ranges.length, TimeAspectDateRange[].class),
                                             super.createWeight(searcher, scoreMode, boost));
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof TimeAspectDateRangeQuery))
            return false;

        return super.equals(obj);
    }
}
