package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;


public class BibleRangeParser extends QParser {
    private final static String QUERY_SEPARATOR = ",";
    private final static String DB_FIELD_SEPARATOR = ",";

    private Query innerQuery;

    public BibleRangeParser(final String searchString, final SolrParams localParams, final SolrParams params, final SolrQueryRequest request) {
        super(searchString, localParams, params, request);
        try {
            final String queryString = "bible_ranges:" + getBookPrefixQueryString(searchString);
            final QParser parser = getParser(queryString, "lucene", getReq());
            final BibleRange[] ranges = getRangesFromQuery();
            this.innerQuery = new BibleRangeQuery(new FilteredQuery(parser.parse(), new BibleRangeFilter(ranges)), ranges);
        } catch (SyntaxError ex) {
            throw new RuntimeException("error parsing query", ex);
        }
    }

    /**
     * Tries to extract the book index of a search query.
     * Then creates a query string only matching bible references starting with the book index.
     * If no book index is found, only '*' will be returned.
     *
     * The first two digits of a range are the book index.
     * See /var/lib/tuelib/books_of_the_bible_to_code.map
     *
     * @param queryString The search string from user
     * @return e.g.  ".*(11|12|12|03)[0-9]{5}.*" (NB. the SOLR query parser anchors regular expressions at the
     * beginning and at the end) or "*"
     */
    private String getBookPrefixQueryString(final String queryString) {
        if (queryString == null || queryString.length() < 2) {
            return "*";
        }
        final String[] ranges = getFieldsFromQuery();
        // Capacity of buffer: (number of ranges) times (two digits of book and one delimiter)
        StringBuilder buffer = new StringBuilder(ranges.length * 3);
        for (String range : ranges) {
            buffer.append('|');
            buffer.append(range.substring(0, 2));
        }
        return "/.*(" + buffer.toString().substring(1) + ")[0-9]{5}.*/";
    }

    @Override
    public Query parse() throws SyntaxError {
        return this.innerQuery;
    }

    private BibleRange[] getRangesFromQuery() {
        return BibleRange.getRanges(getFieldsFromQuery());
    }

    private String[] getFieldsFromQuery() {
        return qstr.split(QUERY_SEPARATOR);
    }

    public static BibleRange[] getRangesFromDatabaseField(final String db_field) {
        return BibleRange.getRanges(db_field, DB_FIELD_SEPARATOR);
    }
}
