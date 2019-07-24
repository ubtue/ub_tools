package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import java.util.Set;
import java.util.TreeSet;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class TimeAspectRangeParser extends QParser {
    private final static String QUERY_SEPARATOR = " ";
    private final static String DB_FIELD_SEPARATOR = ",";
    private static Logger logger = LoggerFactory.getLogger(TimeAspectRangeParser.class);

    /**
     * @param qstr        The part of the query string specific to this parser
     * @param localParams The set of parameters that are specific to this QParser.
     *                    See http://wiki.apache.org/solr/LocalParams
     * @param params      The rest of the {@link SolrParams}
     * @param req         The original {@link SolrQueryRequest}.
     */
    public TimeAspectRangeParser(final String qstr, final SolrParams localParams, final SolrParams params, final SolrQueryRequest req) {
        super(qstr, localParams, params, req);
    }

    static TimeAspectRange[] getRangesFromDatabaseField(final String db_field) {
        return TimeAspectRange.getRanges(db_field, DB_FIELD_SEPARATOR);
    }

    private TimeAspectRange[] getRangesFromQuery() {
        return TimeAspectRange.getRanges(getFieldsFromQuery());
    }

    private String[] getFieldsFromQuery() {
        return qstr.split(QUERY_SEPARATOR);
    }

    // @return true if "queryString" is of the form 07000000_08999999 o/w we return false.
    private boolean isTimeAspectRange(final String queryString) {
        if (queryString.length() != TimeAspectRange.TIME_ASPECT_CODE_LENGTH + 1 + TimeAspectRange.TIME_ASPECT_CODE_LENGTH
            || queryString.charAt(TimeAspectRange.TIME_ASPECT_CODE_LENGTH) != '_')
            return false;
        if (!queryString.substring(1, TimeAspectRange.TIME_ASPECT_CODE_LENGTH).equals("00000000")
            || !queryString.substring(TimeAspectRange.TIME_ASPECT_CODE_LENGTH + 1 + 1,
                                      TimeAspectRange.TIME_ASPECT_CODE_LENGTH + 1 + TimeAspectRange.TIME_ASPECT_CODE_LENGTH).equals("99999999"))
            return false;
        int firstCodexDigit, secondCodexDigit;
        try {
            firstCodexDigit  = Integer.parseInt(queryString.substring(0, 1));
            secondCodexDigit = Integer.parseInt(queryString.substring(TimeAspectRange.TIME_ASPECT_CODE_LENGTH + 1,
                                                                      TimeAspectRange.TIME_ASPECT_CODE_LENGTH + 1 + 1));
        } catch (NumberFormatException e) {
            return false;
        }

        return (firstCodexDigit == 1 || firstCodexDigit == 2 || firstCodexDigit == 3) && secondCodexDigit == firstCodexDigit;
    }

    /**
     * Tries to extract the codex index of a search query.
     * Then creates a query string only matching canon law references starting with the codex index.
     * If no codex index is found, only '*' will be returned.
     *
     * The first digit of a range is the codex index.
     *
     * @param queryString The search string from user
     * @return e.g.  ".*(1|2)[0-9]{8}.*" (NB. the Solr query parser anchors regular expressions at the
     * beginning and at the end) or "*"
     */
    private String getTimeAspectPrefixQueryString(final String queryString) {
        if (queryString == null || queryString.length() < 1) {
            return "*";
        }
        if (isTimeAspectRange(queryString)) {
            return "/.*" + queryString + ".*/";
        }
        final String[] ranges = getFieldsFromQuery();
        final Set<String> alreadySeenCodexDigits = new TreeSet<String>();
        // Capacity of buffer: (number of ranges) times (one digit for the codex and one for the delimiter)
        StringBuilder buffer = new StringBuilder(ranges.length * 2);
        for (String range : ranges) {
            final String firstCodexDigit = range.substring(0, 1);
            if (!alreadySeenCodexDigits.contains(firstCodexDigit)) {
                buffer.append("|" + firstCodexDigit);
                alreadySeenCodexDigits.add(firstCodexDigit);
            }
            final String secondCodexDigit = range.substring(TimeAspectRange.TIME_ASPECT_CODE_LENGTH + 1,
                                                            TimeAspectRange.TIME_ASPECT_CODE_LENGTH + 1 + 1);
            if (!alreadySeenCodexDigits.contains(secondCodexDigit)) {
                buffer.append("|" + secondCodexDigit);
                alreadySeenCodexDigits.add(secondCodexDigit);
            }
        }
        return "/.*(" + buffer.toString().substring(1) + ")[0-9]{8}.*/";
    }

    @Override
    public Query parse() throws SyntaxError {
        final String queryString = "canon_law_ranges:" + getTimeAspectPrefixQueryString(getString());
        final QParser parser = getParser(queryString, "lucene", getReq());
        final TimeAspectRange[] ranges = getRangesFromQuery();
        return new TimeAspectRangeQuery(parser.parse(), ranges);
    }
}
