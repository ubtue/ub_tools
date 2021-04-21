package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import java.io.IOException;
import java.util.Arrays;

import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.Weight;

import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeQuery;


class BibleRangeQuery extends RangeQuery {
    BibleRangeQuery(final Query query, final BibleRange[] ranges) {
        super(query, Arrays.copyOf(ranges, ranges.length, Range[].class));
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final boolean needsScores, final float boost) throws IOException {
        return new BibleRangeWeight(this, Arrays.copyOf(ranges, ranges.length, BibleRange[].class),
                                    super.createWeight(searcher, needsScores, boost));
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof BibleRangeQuery))
            return false;

        return super.equals(obj);
    }
}
