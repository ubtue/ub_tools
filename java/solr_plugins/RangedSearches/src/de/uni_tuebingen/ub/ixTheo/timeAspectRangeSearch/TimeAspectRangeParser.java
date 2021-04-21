package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;


public class TimeAspectRangeParser extends QParser {
    private final static String QUERY_SEPARATOR = " ";
    private final static String DB_FIELD_SEPARATOR = ",";
    //private static Logger logger = LoggerFactory.getLogger(TimeAspectRangeParser.class);

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

    @Override
    public Query parse() throws SyntaxError {
        final String queryString = "time_aspect_ranges:*";
        final QParser parser = getParser(queryString, "multiLanguageQueryParser", getReq());
        final TimeAspectRange[] ranges = getRangesFromQuery();
        return new TimeAspectRangeQuery(parser.parse(), ranges);
    }
}
