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
    public Weight createWeight(final IndexSearcher searcher, final boolean needsScores, final float boost) throws IOException {
        // query rewriting is necessary before createWeight delivers any usable result.
        // rewrite needs to be called multiple times, until derived Query class no longer changes.
        // see https://issues.apache.org/jira/browse/LUCENE-6785?jql=text%20~%20%22createWeight%22
        Query rewrite_query = BibleRangeQuery.this.query;
        Query rewritten_query = rewrite_query;
        do {
            rewrite_query = rewritten_query;
            rewritten_query = rewrite_query.rewrite(searcher.getIndexReader());
        } while(rewrite_query.getClass() != rewritten_query.getClass());

        final Weight weight = rewritten_query.createWeight(searcher, needsScores, boost);
        return new BibleRangeWeight(this, ranges, weight);
    }

    @Override
    public String toString(String default_field) {
        return query.toString(default_field);
    }

    // The standard toString() in the parent class is final so we needed to give this a different name.
    public String asString() {
        return "BibleRangeQuery(Query:" + query.toString() + ", Ranges:" + Range.toString(ranges) + ")";
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
