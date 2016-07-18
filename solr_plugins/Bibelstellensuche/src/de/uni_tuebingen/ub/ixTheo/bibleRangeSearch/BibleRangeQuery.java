package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.Weight;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;


class BibleRangeQuery extends Query {
    private static Logger logger = LoggerFactory.getLogger(BibleRangeQuery.class);

    private Query query;
    private BibleRange[] ranges;

    BibleRangeQuery(final Query query, final BibleRange[] ranges) {
        this.query = query;
        this.ranges = ranges;
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final boolean needsScores) throws IOException {
        final Weight weight = BibleRangeQuery.this.query.rewrite(searcher.getIndexReader()).createWeight(searcher, needsScores);
        return new BibleRangeWeight(this, ranges, weight);
    }

    @Override
    public String toString(final String s) {
        return "BibleRangeQuery(" + s + ")";
    }

    /**
     * We don't want to get cached. Otherwise we will get the same results regardless of the query.
     *
     * @param obj the other object
     * @return always false.
     */
    @Override
    public boolean equals(final Object obj) {
        return false;
    }

    /**
     * We don't want to get cached. Otherwise we will get the same results regardless of the query.
     *
     * @return a random number.
     */
    @Override
    public int hashCode() {
        return (int) (Math.random() * Integer.MAX_VALUE);
    }
}
