package de.uni_tuebingen.ub.ixTheo.multiLanguageQuery;

import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.params.ModifiableSolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.schema.IndexSchema;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.apache.commons.lang.StringUtils;
import org.apache.commons.lang.ArrayUtils;


public class MultiLanguageQueryParser extends QParser {

    protected Query query;
    protected String searchString;
    protected static Logger logger = LoggerFactory.getLogger(MultiLanguageQueryParser.class);
    protected String[] SUPPORTED_LANGUAGES = { "de", "en", "fr", "it", "es", "hant", "hans" };
    protected SolrQueryRequest newRequest;
    protected ModifiableSolrParams newParams;

    public MultiLanguageQueryParser(final String searchString, final SolrParams localParams, final SolrParams params,
            final SolrQueryRequest request) throws MultiLanguageQueryParserException
    {
        super(searchString, localParams, params, request);
        this.searchString = searchString;
        newRequest = request;
        this.newParams = new ModifiableSolrParams();
        this.newParams.add(params);
        IndexSchema schema = request.getSchema();
        Boolean useDismax = false;
        String[] query = newParams.getParams("q");
        String[] queryFields;

        // Check whether we have dismax or edismax
        String[] queryType = newParams.getParams("qt");
        if (queryType != null) {
            queryFields = newParams.getParams("qf");
            useDismax = true;
        }
        else {
            if (query.length != 1)
               throw new MultiLanguageQueryParserException("Only one q-parameter is supported");
            final String[] separatedFields = query[0].split(":");
            if (separatedFields.length == 2)
               queryFields = new String[]{ separatedFields[0] };
            else
               throw new MultiLanguageQueryParserException(
                         "Currently only a single query field is supported by the Lucene parser");
        }

        String[] facetFields = newParams.getParams("facet.field");
        String lang = newParams.get("lang", "de");

        // Strip language subcode
        lang = lang.split("-")[0];

        // Set default language if we do not directly support the chosen
        // language
        lang = ArrayUtils.contains(SUPPORTED_LANGUAGES, lang) ? lang : "de";

        for (String param : queryFields) {
            if (useDismax) {
               newParams.remove("qf", param);
                 String[] singleParams = param.split(" ");
                 StringBuilder sb = new StringBuilder();
                 int i = 0;
                 for (String singleParam : singleParams) {
                     String newFieldName = singleParam + "_" + lang;
                     newFieldName = (schema.getFieldOrNull(newFieldName) != null) ? newFieldName : singleParam;
                     sb.append(newFieldName);
                     if (++i < singleParams.length)
                         sb.append(" ");
                 }
                 newParams.add("qf", sb.toString());
            }
            // Restricted support for lucene parser
            else {
                final String queryField = queryFields[0];
                String newFieldName = queryField + "_" + lang;
                if (query.length == 1)
                    newParams.add("q", query[0].replace(queryField, newFieldName));
                else
                    throw new MultiLanguageQueryParserException("Only one q-parameter is supported [1]");
            }
        }

        if (facetFields != null && facetFields.length > 0) {
            for (String param : facetFields) {
                // Replace field used if it exists
                String newFieldName = param + "_" + lang;
                if (schema.getFieldOrNull(newFieldName) != null) {
                    newParams.remove("facet.field", param);
                    if (useDismax)
                        newParams.add("facet.field", newFieldName);
                    else {
                        newParams.add("facet.field", "{!key=" + param + "}" + newFieldName);
                    }
                }
            }
        }
    }

    public Query parse() throws SyntaxError {
        this.newRequest.setParams(newParams);
        QParser parser = getParser(this.searchString, "edismax", this.newRequest);
        return parser.parse();
    }
}
