package de.uni_tuebingen.ub.ixTheo.canonesDateRangeSearch;


import java.io.IOException;
import java.util.Arrays;

import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.ScoreMode;
import org.apache.lucene.search.Weight;

import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeQuery;


class CanonesDateRangeQuery extends RangeQuery {
    CanonesDateRangeQuery(final Query query, final CanonesDateRange[] ranges) {
        super(query, Arrays.copyOf(ranges, ranges.length, Range[].class));
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final ScoreMode scoreMode, final float boost) throws IOException {
        return new CanonesDateRangeWeight(this, Arrays.copyOf(ranges, ranges.length, CanonesDateRange[].class),
                                          super.createWeight(searcher, scoreMode, boost));
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof CanonesDateRangeQuery))
            return false;

        return super.equals(obj);
    }
}
