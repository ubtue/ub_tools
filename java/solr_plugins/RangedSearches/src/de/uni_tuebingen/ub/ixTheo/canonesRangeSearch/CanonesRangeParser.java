package de.uni_tuebingen.ub.ixTheo.canonesRangeSearch;


import java.util.Set;
import java.util.TreeSet;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class CanonesRangeParser extends QParser {
    private final static String QUERY_SEPARATOR = " ";
    private final static String DB_FIELD_SEPARATOR = ",";
    private static Logger logger = LoggerFactory.getLogger(CanonesRangeParser.class);

    /**
     * @param qstr        The part of the query string specific to this parser
     * @param localParams The set of parameters that are specific to this QParser.
     *                    See http://wiki.apache.org/solr/LocalParams
     * @param params      The rest of the {@link SolrParams}
     * @param req         The original {@link SolrQueryRequest}.
     */
    public CanonesRangeParser(final String qstr, final SolrParams localParams, final SolrParams params, final SolrQueryRequest req) {
        super(qstr, localParams, params, req);
    }

    static CanonesRange[] getRangesFromDatabaseField(final String db_field) {
        return CanonesRange.getRanges(db_field, DB_FIELD_SEPARATOR);
    }

    private CanonesRange[] getRangesFromQuery() {
        return CanonesRange.getRanges(getFieldsFromQuery());
    }

    private String[] getFieldsFromQuery() {
        return qstr.split(QUERY_SEPARATOR);
    }

    // @return true if "queryString" is of the form 07000000_08999999 o/w we return false.
    private boolean isCanonesRange(final String queryString) {
        if (queryString.length() != 9 + 1 + 9 || queryString.charAt(9) != '_')
            return false;
        if (!queryString.substring(2, 9).equals("0000000") || !queryString.substring(12, 19).equals("9999999"))
            return false;
        int firstCodexDigit, secondCodexDigit;
        try {
            firstCodexDigit  = Integer.parseInt(queryString.substring(0, 1));
            secondCodexDigit = Integer.parseInt(queryString.substring(10, 11));
        } catch (NumberFormatException e) {
            return false;
        }

        return (firstCodexDigit == 1 || firstCodexDigit == 2 || firstCodexDigit == 3) && secondCodexDigit == firstCodexDigit;
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
    private String getCanonesPrefixQueryString(final String queryString) {
        if (queryString == null || queryString.length() < 1) {
            return "*";
        }
        if (isCanonesRange(queryString)) {
            return "/.*" + queryString + ".*/";
        }
        final String[] ranges = getFieldsFromQuery();
        final Set<String> alreadySeenCodexDigits = new TreeSet<String>();
        // Capacity of buffer: (number of ranges) times (two digits of book and one delimiter)
        StringBuilder buffer = new StringBuilder(ranges.length * 3);
        for (String range : ranges) {
            final String firstCodexDigit = range.substring(0, 2);
            if (!alreadySeenCodexDigits.contains(firstCodexDigit)) {
                buffer.append("|" + firstCodexDigit);
                alreadySeenCodexDigits.add(firstCodexDigit);
            }
            final String secondCodexDigit = range.substring(9, 11);
            if (!alreadySeenCodexDigits.contains(secondCodexDigit)) {
                buffer.append("|" + secondCodexDigit);
                alreadySeenCodexDigits.add(secondCodexDigit);
            }
        }
        return "/.*(" + buffer.toString().substring(1) + ")[0-9]{6}.*/";
    }

    @Override
    public Query parse() throws SyntaxError {
        final String queryString = "bible_ranges:" + getCanonesPrefixQueryString(getString());
        final QParser parser = getParser(queryString, "lucene", getReq());
        final CanonesRange[] ranges = getRangesFromQuery();
        return new CanonesRangeQuery(parser.parse(), ranges);
    }
}
