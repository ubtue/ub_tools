package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import java.util.Set;
import java.util.TreeSet;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class BibleRangeParser extends QParser {
    private final static String QUERY_SEPARATOR = " ";
    private final static String DB_FIELD_SEPARATOR = ",";
    private static Logger logger = LoggerFactory.getLogger(BibleRangeParser.class);

    /**
     * Constructor for the QParser
     *
     * @param qstr        The part of the query string specific to this parser
     * @param localParams The set of parameters that are specific to this QParser.
     *                    See http://wiki.apache.org/solr/LocalParams
     * @param params      The rest of the {@link SolrParams}
     * @param req         The original {@link SolrQueryRequest}.
     */
    public BibleRangeParser(final String qstr, final SolrParams localParams, final SolrParams params,
                            final SolrQueryRequest req)
    {
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

    // @return true if "queryString" is of the form 07000000_08999999 o/w we return false.
    private boolean isBookRange(final String queryString) {
        if (queryString.length() != 8 + 1 + 8 || queryString.charAt(8) != '_')
            return false;
        if (!queryString.substring(2, 8).equals("000000") || !queryString.substring(11, 17).equals("999999"))
            return false;
        int firstBookCode, secondBookCode;
        try {
            firstBookCode  = Integer.parseInt(queryString.substring(0, 2));
            secondBookCode = Integer.parseInt(queryString.substring(9, 11));
        } catch (NumberFormatException e) {
            return false;
        }

        return secondBookCode - firstBookCode >= 1;
    }

    /**
     * Tries to extract the book index of a search query.
     * Then creates a query string only matching bible references starting with the book index.
     * If no book index is found, only '*' will be returned.
     *
     * The first two digits of a range are the book index.
     * See /usr/local/var/lib/tuelib/books_of_the_bible_to_code.map
     *
     * @param queryString The search string from user
     * @return e.g.  ".*(11|12|03)[0-9]{6}.*" (NB. the Solr query parser anchors regular expressions at the
     * beginning and at the end) or "*"
     */
    private String getBookPrefixQueryString(final String queryString) {
        if (queryString == null || queryString.length() < 2) {
            return "*";
        }
        if (isBookRange(queryString)) {
            return "/.*" + queryString + ".*/";
        }
        final String[] ranges = getFieldsFromQuery();
        final Set<String> alreadySeenBookCodes = new TreeSet<String>();
        // Capacity of buffer: (number of ranges) times (two digits of book and one delimiter)
        StringBuilder buffer = new StringBuilder(ranges.length * 3);
        for (String range : ranges) {
            final String firstBookCode = range.substring(0, 2);
            if (!alreadySeenBookCodes.contains(firstBookCode)) {
                buffer.append("|" + firstBookCode);
                alreadySeenBookCodes.add(firstBookCode);
            }
            final String secondBookCode = range.substring(9, 11);
            if (!alreadySeenBookCodes.contains(secondBookCode)) {
                buffer.append("|" + secondBookCode);
                alreadySeenBookCodes.add(secondBookCode);
            }
        }
        return "/.*(" + buffer.toString().substring(1) + ")[0-9]{6}.*/";
    }

    @Override
    public Query parse() throws SyntaxError {
        final String queryString = "bible_ranges:" + getBookPrefixQueryString(getString());
        final QParser parser = getParser(queryString, "lucene", getReq());
        final BibleRange[] ranges = getRangesFromQuery();
        return new BibleRangeQuery(parser.parse(), ranges);
    }
}
