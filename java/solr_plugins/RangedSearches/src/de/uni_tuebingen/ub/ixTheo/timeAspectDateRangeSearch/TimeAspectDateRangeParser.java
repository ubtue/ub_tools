package de.uni_tuebingen.ub.ixTheo.timeAspectDateRangeSearch;


import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeParser;


public class TimeAspectDateRangeParser extends RangeParser {

    /**
     * @param qstr        The part of the query string specific to this parser
     * @param localParams The set of parameters that are specific to this QParser.
     *                    See http://wiki.apache.org/solr/LocalParams
     * @param params      The rest of the {@link SolrParams}
     * @param req         The original {@link SolrQueryRequest}.
     */
    public TimeAspectDateRangeParser(final String qstr, final SolrParams localParams, final SolrParams params, final SolrQueryRequest req) {
        super(qstr, localParams, params, req);
    }

    static TimeAspectDateRange[] getRangesFromDatabaseField(final String db_field) {
        return TimeAspectDateRange.getRanges(db_field, DB_FIELD_SEPARATOR);
    }

    private TimeAspectDateRange[] getRangesFromQuery() {
        return TimeAspectDateRange.getRanges(getFieldsFromQuery());
    }

    private String[] getFieldsFromQuery() {
        return qstr.split(QUERY_OR_SEPARATOR);
    }

    @Override
    public Query parse() throws SyntaxError {
        final String queryString = "time_aspect_date_ranges:*";
        final QParser parser = getParser(queryString, "multiLanguageQueryParser", getReq());
        final TimeAspectDateRange[] ranges = getRangesFromQuery();
        return new TimeAspectDateRangeQuery(parser.parse(), ranges);
    }
}
