package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeParser;


public class TimeAspectRangeParser extends RangeParser {
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
        return qstr.split(QUERY_FIELD_SEPARATOR);
    }

    @Override
    public Query parse() throws SyntaxError {

        String queryString = "time_aspect_ranges:*";
        final TimeAspectRange[] ranges = getRangesFromQuery();

        //if more than one time range become relevant, minimum lower and
        //maximum upper need to be implemented for pre-selection
        if (ranges.length != 1) {
            throw new SyntaxError("Unsupported format");
        }
        TimeAspectRange range = ranges[0];
        String lower = String.format("%012d", range.getLower());
        String upper = String.format("%012d", range.getUpper());
        queryString = "time_range_end:[" + lower + " TO " + "*" + "] AND time_range_start:[" + "*" + " TO " + upper + "]";
        final QParser parser = getParser(queryString, "multiLanguageQueryParser", getReq());
        return new TimeAspectRangeQuery(parser.parse(), ranges);
    }
}
