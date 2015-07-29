package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;

public class BibleRangeParser extends QParser {
    private final static String QUERY_SEPARATOR = " ";
    private final static String DB_FIELD_SEPARATOR = ",";

    private Query innerQuery;

    public BibleRangeParser(final String queryString, final SolrParams localParams, final SolrParams params, final SolrQueryRequest request) {
        super(queryString, localParams, params, request);
        try {
            final QParser parser = getParser("+bible_ranges:*", "lucene", getReq());
            final Range[] ranges = getRangesFromQuery();
            this.innerQuery = new FilteredQuery(new BibleRangeQuery(parser.parse(), ranges), new BibleRangeFilter(ranges));
        } catch (SyntaxError ex) {
            throw new RuntimeException("error parsing query", ex);
        }
    }

    @Override
    public Query parse() throws SyntaxError {
        return this.innerQuery;
    }

    private Range[] getRangesFromQuery() {
        return BibleRange.getRanges(getFieldsFromQuery());
    }

    private String[] getFieldsFromQuery() {
        return qstr.split(QUERY_SEPARATOR);
    }

    public static Range[] getRangesFromDatabaseField(final String db_field) {
        return BibleRange.getRanges(db_field, DB_FIELD_SEPARATOR);
    }
}
