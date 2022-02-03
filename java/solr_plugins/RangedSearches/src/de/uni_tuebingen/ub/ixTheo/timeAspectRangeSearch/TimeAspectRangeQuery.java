package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import java.io.IOException;
import java.util.Arrays;

import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.ScoreMode;
import org.apache.lucene.search.Weight;

import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeQuery;


class TimeAspectRangeQuery extends RangeQuery {
    TimeAspectRangeQuery(final Query query, final TimeAspectRange[] ranges) {
        super(query, Arrays.copyOf(ranges, ranges.length, Range[].class));
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final ScoreMode scoreMode, final float boost) throws IOException {
        return new TimeAspectRangeWeight(this, Arrays.copyOf(ranges, ranges.length, TimeAspectRange[].class),
                                         super.createWeight(searcher, scoreMode, boost));
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof TimeAspectRangeQuery))
            return false;

        return super.equals(obj);
    }
}
