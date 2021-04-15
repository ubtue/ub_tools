package de.uni_tuebingen.ub.ixTheo.rangeSearch;


import java.io.IOException;
import java.time.DateTimeException;
import java.time.Instant;

import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.Weight;


public class DateRangeQuery extends Query {
    //private static Logger logger = LoggerFactory.getLogger(DateRangeQuery.class);

    private Query query;
    protected Range[] ranges;

    public DateRangeQuery(final Query query, final String date_ranges) {
        this.query = query;
        this.ranges = convertDateRangesToIntegerRanges(date_ranges);
    }

    @Override
    public Weight createWeight(final IndexSearcher searcher, final boolean needsScores, final float boost) throws IOException {
        // query rewriting is necessary before createWeight delivers any usable result.
        // rewrite needs to be called multiple times, until the derived Query class no longer changes.
        // see https://issues.apache.org/jira/browse/LUCENE-6785?jql=text%20~%20%22createWeight%22
        Query rewrite_query = DateRangeQuery.this.query;
        Query rewritten_query = rewrite_query;
        do {
            rewrite_query = rewritten_query;
            rewritten_query = rewrite_query.rewrite(searcher.getIndexReader());
        } while (rewrite_query.getClass() != rewritten_query.getClass());

        return rewritten_query.createWeight(searcher, needsScores, boost);
    }

    @Override
    public String toString(String default_field) {
        return query.toString(default_field);
    }

    // The standard toString() in the parent class is final so we needed to give this a different name.
    public String asString() {
        return "DateRangeQuery(Query:" + query.toString() + ", Ranges:" + Range.toString(ranges) + ")";
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof DateRangeQuery))
            return false;

        final DateRangeQuery otherQuery = (DateRangeQuery)obj;
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

    private static long convertDateStringToLong(final String date) {
        try {
            return Instant.parse(date).getEpochSecond();
        } catch (final DateTimeException x) {
            System.err.println("in DateRangeQuery.convertDateStringToLong: " + date + " is a malformed date!");
            System.exit(-1);
            return 0; // Keep the compiler happy!
        }
    }

    // Converts a date range to an integer range (= our internal Range type
    // corresponding to the range data in our MARC records).
    private static Range getIntegerRange(String date_range) {
        if (!date_range.startsWith("[") || !date_range.endsWith("]")) {
            System.err.println(date_range + " is a malformed date range! (1)");
            System.exit(-1);
        }
        date_range = date_range.substring(1, date_range.length() - 1);

        final String[] dates = date_range.split(" TO ");
        if (dates.length != 2) {
            System.err.println(date_range + " is a malformed date range! (2)");
            System.exit(-1);
        }

        final long lower = convertDateStringToLong(dates[0]);
        final long upper = convertDateStringToLong(dates[0]);
        return new Range(lower, upper);
    }

    // "date_ranges_str" consists of one or more date ranges in spquare brackets connected with boolean ORs.
    private static Range[] convertDateRangesToIntegerRanges(final String date_ranges_str) {
        final String[] date_ranges = date_ranges_str.split(" OR ");
        final Range[] integerRanges = new Range[date_ranges.length];
        for (int i = 0; i < date_ranges.length; ++i)
            integerRanges[i] = getIntegerRange(date_ranges[i]);
        return integerRanges;
    }
}
