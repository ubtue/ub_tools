package de.uni_tuebingen.ub.ixTheo.rangeSearch;


import java.io.IOException;

import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.Weight;


public class RangeQuery extends Query {
    //private static Logger logger = LoggerFactory.getLogger(RangeQuery.class);

    private Query query;
    protected Range[] ranges;

    public RangeQuery(final Query query, final Range[] ranges) {
        this.query = query;
        this.ranges = ranges;
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final boolean needsScores, final float boost) throws IOException {
        // query rewriting is necessary before createWeight delivers any usable result.
        // rewrite needs to be called multiple times, until the derived Query class no longer changes.
        // see https://issues.apache.org/jira/browse/LUCENE-6785?jql=text%20~%20%22createWeight%22
        Query rewrite_query = RangeQuery.this.query;
        Query rewritten_query = rewrite_query;
        do {
            rewrite_query = rewritten_query;
            rewritten_query = rewrite_query.rewrite(searcher.getIndexReader());
        } while(rewrite_query.getClass() != rewritten_query.getClass());

        return rewritten_query.createWeight(searcher, needsScores, boost);
    }

    @Override
    public String toString(String default_field) {
        return query.toString(default_field);
    }

    // The standard toString() in the parent class is final so we needed to give this a different name.
    public String asString() {
        return "RangeQuery(Query:" + query.toString() + ", Ranges:" + Range.toString(ranges) + ")";
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof RangeQuery))
            return false;

        final RangeQuery otherQuery = (RangeQuery)obj;
        if (otherQuery.ranges.length != ranges.length)
            return false;

        for (int i = 0; i < ranges.length; ++i) {
            if (!ranges[i].equals(otherQuery.ranges[i]))
                return false;
        }

        return true;
    }

    @Override
    public int hashCode() {
        int combinedHashCode = 0;
        for (final Range range : ranges)
            combinedHashCode ^= range.hashCode();
        return combinedHashCode;
    }
}
