package de.uni_tuebingen.ub.ixTheo.bibleDateRangeSearch;


import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeParser;


public class BibleDateRangeParser extends RangeParser {

    /**
     * Constructor for the QParser
     *
     * @param qstr        The part of the query string specific to this parser
     * @param localParams The set of parameters that are specific to this QParser.
     *                    See http://wiki.apache.org/solr/LocalParams
     * @param params      The rest of the {@link SolrParams}
     * @param req         The original {@link SolrQueryRequest}.
     */
    public BibleDateRangeParser(final String qstr, final SolrParams localParams, final SolrParams params,
                                final SolrQueryRequest req)
    {
        super(qstr, localParams, params, req);
    }

    static BibleDateRange[] getRangesFromDatabaseField(final String db_field) {
        return BibleDateRange.getRanges(db_field, DB_FIELD_SEPARATOR);
    }

    private BibleDateRange[] getRangesFromQuery() {
        return BibleDateRange.getRanges(qstr, QUERY_OR_SEPARATOR);
    }

    @Override
    public Query parse() throws SyntaxError {
        final String queryString = "bible_date_ranges:" + getString();
        final QParser parser = getParser(queryString, "multiLanguageQueryParser", getReq());
        final BibleDateRange[] ranges = getRangesFromQuery();
        return new BibleDateRangeQuery(parser.parse(), ranges);
    }
}
