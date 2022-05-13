package de.uni_tuebingen.ub.ixTheo.bibleDateRangeSearch;


import java.io.IOException;
import java.util.Arrays;

import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.ScoreMode;
import org.apache.lucene.search.Weight;

import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeQuery;


class BibleDateRangeQuery extends RangeQuery {
    BibleDateRangeQuery(final Query query, final BibleDateRange[] ranges) {
        super(query, Arrays.copyOf(ranges, ranges.length, Range[].class));
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final ScoreMode scoreMode, final float boost) throws IOException {
        return new BibleDateRangeWeight(this, Arrays.copyOf(ranges, ranges.length, BibleDateRange[].class),
                                        super.createWeight(searcher, scoreMode, boost));
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof BibleDateRangeQuery))
            return false;

        return super.equals(obj);
    }
}
