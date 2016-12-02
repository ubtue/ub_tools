package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class BibleRangeParser extends QParser {
    private final static String QUERY_SEPARATOR = ",";
    private final static String DB_FIELD_SEPARATOR = ",";
    private static Logger logger = LoggerFactory.getLogger(BibleRangeParser.class);

    /**
     * Constructor for the QParser
     *
     * @param qstr        The part of the query string specific to this parser
     * @param localParams The set of parameters that are specific to this QParser.  See http://wiki.apache.org/solr/LocalParams
     * @param params      The rest of the {@link SolrParams}
     * @param req         The original {@link SolrQueryRequest}.
     */
    public BibleRangeParser(final String qstr, final SolrParams localParams, final SolrParams params, final SolrQueryRequest req) {
        super(qstr, localParams, params, req);
    }

    static BibleRange[] getRangesFromDatabaseField(final String db_field) {
        return BibleRange.getRanges(db_field, DB_FIELD_SEPARATOR);
    }

    private BibleRange[] getRangesFromQuery() {
        return BibleRange.getRanges(getFieldsFromQuery());
    }

    private String[] getFieldsFromQuery() {
        return qstr.split(QUERY_SEPARATOR);
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
            final String firstBook = range.substring(0, 2);
            final String secondBook = range.substring(8, 10);
            buffer.append('|');
            buffer.append(firstBook);
            if (!firstBook.equals(secondBook)) {
                buffer.append('|');
                buffer.append(secondBook);
            }
        }
        return "/.*(" + buffer.toString().substring(1) + ")[0-9]{5}.*/";
    }

    @Override
    public Query parse() throws SyntaxError {
        final String queryString = "bible_ranges:" + getBookPrefixQueryString(getString());
        final QParser parser = getParser(queryString, "lucene", getReq());
        final BibleRange[] ranges = getRangesFromQuery();
        return new BibleRangeQuery(parser.parse(), ranges);
    }
}
