package de.uni_tuebingen.ub.ixTheo.multiLanguageQuery;

import java.io.IOException;
import java.util.Iterator;
import java.util.Set;
import org.apache.lucene.index.Term;
import org.apache.lucene.search.BooleanClause;
import org.apache.lucene.search.BooleanQuery;
import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.TermQuery;
import org.apache.lucene.search.TermRangeQuery;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.params.ModifiableSolrParams;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.SolrException.ErrorCode;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.schema.IndexSchema;
import org.apache.solr.search.LuceneQParser;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import org.apache.solr.servlet.SolrRequestParsers;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.apache.commons.lang.ArrayUtils;
import org.apache.commons.lang.StringUtils;


public class MultiLanguageQueryParser extends QParser {
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
            queryFields = separatedFields;
        }

        String[] facetFields = newParams.getParams("facet.field");
        String lang = newParams.get("lang", "de");

        // Strip language subcode
        lang = lang.split("-")[0];

        // Set default language if we do not directly support the chosen
        // language
        lang = ArrayUtils.contains(SUPPORTED_LANGUAGES, lang) ? lang : "de";

        if (useDismax) {
           StringBuilder stringBuilder = new StringBuilder();
           for (final String param : queryFields) {
               newParams.remove("qf", param);
               String[] singleParams = param.split(" ");
               int i = 0;
               for (final String singleParam : singleParams) {
                   String newFieldName = singleParam + "_" + lang;
                   newFieldName = (schema.getFieldOrNull(newFieldName) != null) ? newFieldName : singleParam;
                   stringBuilder.append(newFieldName);
                   if (++i < singleParams.length)
                       stringBuilder.append(" ");
               }
            }
            newParams.add("qf", stringBuilder.toString());
        }
        // Support for Lucene parser
        else {
            if (query.length == 1) {
                try {
                   QParser tmpParser = new LuceneQParser(searchString, localParams, params, request);
                   Query myQuery = tmpParser.getQuery();
                   myQuery = myQuery.rewrite(request.getSearcher().getIndexReader());
                   if (myQuery instanceof BooleanQuery) {
                         Iterator<BooleanClause> boolIterator = ((BooleanQuery) myQuery).iterator();
                         BooleanQuery.Builder queryBuilder = new BooleanQuery.Builder();
                         while (boolIterator.hasNext()) {
                             BooleanClause currentClause = boolIterator.next();
                             Query termQueryCandidate = currentClause.getQuery();
                             if (termQueryCandidate instanceof TermQuery){
                                 TermQuery termQuery = (TermQuery) termQueryCandidate;
                                 String field = termQuery.getTerm().field();
                                 String newFieldName = field + "_" + lang;
                                 if (schema.getFieldOrNull(newFieldName) != null) {
                                     termQueryCandidate = new TermQuery(new Term(newFieldName, "\"" +
                                                              termQuery.getTerm().text() + "\""));
                                 } else
                                    termQueryCandidate = new TermQuery(new Term(field, "\""  +
                                                              termQuery.getTerm().text() + "\""));
                             } else
                                 logger.warn("No appropriate Query in BooleanClause");
                             queryBuilder.add(termQueryCandidate, currentClause.getOccur());
                        }
                        myQuery = queryBuilder.build();
                   } else if (myQuery instanceof TermRangeQuery) {
                       TermRangeQuery termRangeQuery = (TermRangeQuery) myQuery;
                       String field = termRangeQuery.getField();
                       String newFieldName = field + "_" + lang;
                       if (schema.getFieldOrNull(newFieldName) != null) {
                            termRangeQuery = new TermRangeQuery(newFieldName,
                                                                termRangeQuery.getLowerTerm(),
                                                                termRangeQuery.getUpperTerm(),
                                                                termRangeQuery.includesLower(),
                                                                termRangeQuery.includesUpper());
                            myQuery = termRangeQuery;
                       }
                   } else if (myQuery instanceof TermQuery) {
                       TermQuery termQuery = (TermQuery) myQuery;
                       String field = termQuery.getTerm().field();
                       String newFieldName = field + "_" + lang;
                       if (schema.getFieldOrNull(newFieldName) != null) {
                            field = newFieldName;
                            myQuery = new TermQuery(new Term(newFieldName, "\"" +  termQuery.getTerm().text() + "\""));
                       } else 
                           myQuery = new TermQuery(new Term(field, "\"" +  termQuery.getTerm().text() + "\""));
                   } else
                       logger.warn("No rewrite rule did match for " + myQuery.getClass());
                   this.searchString = myQuery.toString();
                   newParams.set("q", this.searchString);
                } catch(SyntaxError|IOException e) {
                    throw new SolrException(ErrorCode.SERVER_ERROR, "Rewriting Lucene support for new languages failed", e);
                }
            } else
                throw new MultiLanguageQueryParserException("Only one q-parameter is supported [1]");
        }

        // Handle Facet Fields
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
        QParser parser = getParser(this.searchString, null, this.newRequest);
        return parser.parse();
    }
}

