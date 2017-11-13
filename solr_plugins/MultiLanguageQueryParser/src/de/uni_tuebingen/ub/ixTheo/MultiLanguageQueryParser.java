package de.uni_tuebingen.ub.ixTheo.multiLanguageQuery;

import java.io.IOException;
import java.util.Iterator;
import java.util.Set;
import java.util.List;
import java.util.ArrayList;
import org.apache.lucene.index.Term;
import org.apache.lucene.search.BooleanClause;
import org.apache.lucene.search.BooleanQuery;
import org.apache.lucene.search.BoostQuery;
import org.apache.lucene.search.DisjunctionMaxQuery;
import org.apache.lucene.search.PhraseQuery;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.TermQuery;
import org.apache.lucene.search.TermRangeQuery;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.params.ModifiableSolrParams;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.SolrException.ErrorCode;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.schema.IndexSchema;
import org.apache.solr.search.DisMaxQParser;
import org.apache.solr.search.ExtendedDismaxQParser;
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
    protected IndexSchema schema;
    protected String lang;
    protected Query newQuery;


    public MultiLanguageQueryParser(final String searchString, final SolrParams localParams, final SolrParams params,
            final SolrQueryRequest request) throws MultiLanguageQueryParserException
    {
        super(searchString, localParams, params, request);
        this.searchString = searchString;
        newRequest = request;
        this.newParams = new ModifiableSolrParams();
        this.newParams.add(params);
        schema = request.getSchema();
        Boolean useDismax = false;
        String[] query = newParams.getParams("q");
        String[] queryFields = null;

        // Check whether we have dismax or edismax
        String[] queryType = newParams.getParams("qt");
        if (queryType != null) {
            queryFields = newParams.getParams("qf");
            useDismax = true;
        } else if (query.length != 1)
            throw new MultiLanguageQueryParserException("Only one q-parameter is supported");

        String[] facetFields = newParams.getParams("facet.field");
        lang = newParams.get("lang", "de");

        // Strip language subcode
        lang = lang.split("-")[0];

        // Set default language if we do not directly support the chosen
        // language
        lang = ArrayUtils.contains(SUPPORTED_LANGUAGES, lang) ? lang : "de";


        // Handling for [e]dismax
        if (useDismax)
            handleDismaxParser(queryFields, lang, schema);
        // Support for Lucene parser
        else
            handleLuceneParser(query, request, lang, schema);

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


    protected void handleDismaxParser(String[] queryFields, String lang, IndexSchema schema) {
        StringBuilder stringBuilder = new StringBuilder();
        // Only replace parameters if qf is indeed set
        if (newParams.get("qf") == null)
            return;
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
         try {
             this.newRequest.setParams(newParams);
             QParser parser = getParser(this.searchString, "edismax", this.newRequest);
             newQuery = parser.parse();
         } catch (SyntaxError e) {
               throw new SolrException(ErrorCode.SERVER_ERROR, "Could not succesfully rewrite query", e);
         }
    }


    protected Query processTermQuery(TermQuery queryCandidate) {
        final TermQuery termQuery = (TermQuery) queryCandidate;
        final String field = termQuery.getTerm().field();
        final String newFieldName = field + "_" + lang;
        if (schema.getFieldOrNull(newFieldName) != null)
            queryCandidate = new TermQuery(new Term(newFieldName, termQuery.getTerm().text()));
        else
            queryCandidate = new TermQuery(new Term(field, termQuery.getTerm().text()));

        return queryCandidate;
    }


    protected Query processPhraseQuery(PhraseQuery queryCandidate) {
      PhraseQuery.Builder phraseQueryBuilder = new PhraseQuery.Builder();
      for (Term term : queryCandidate.getTerms()) {
          String field = term.field();
          String newFieldName = field + "_" + lang;
          if (schema.getFieldOrNull(newFieldName) != null) {
              phraseQueryBuilder.add(new Term(newFieldName, term.text()));
          } else
              phraseQueryBuilder.add(term);
      }

      phraseQueryBuilder.setSlop(queryCandidate.getSlop());
      return phraseQueryBuilder.build();
    }


    protected Query processDisjunctionMaxQuery(DisjunctionMaxQuery queryCandidate) {
        final List<Query> queryList = new ArrayList<Query>();
        DisjunctionMaxQuery disjunctionMaxQuery = (DisjunctionMaxQuery) queryCandidate;
        for (Query currentClause : disjunctionMaxQuery.getDisjuncts()) {
             if (currentClause instanceof BoostQuery)
                 currentClause = processBoostQuery((BoostQuery)currentClause);
             else if (currentClause instanceof TermQuery)
                 currentClause = processTermQuery((TermQuery)currentClause);
             else if (currentClause instanceof BooleanQuery)
                 currentClause = processBooleanQuery((BooleanQuery)currentClause);
             else
                 throw new SolrException(ErrorCode.SERVER_ERROR, "Unknown currentClause in DisjunctionMaxQuery");

             queryList.add(currentClause);
        }
        return new DisjunctionMaxQuery(queryList, queryCandidate.getTieBreakerMultiplier());
    }


    protected Query processBoostQuery(BoostQuery queryCandidate) {
        final Query subquery = queryCandidate.getQuery();
        if (subquery instanceof TermQuery) {
            subquery = processTermQuery((TermQuery)subquery);
            return new BoostQuery(subquery, queryCandidate.getBoost());
        } else
	    throw new SolrException(ErrorCode.SERVER_ERROR, "Unable to handle " +  subquery.getClass().getName());
    }


    protected Query processBooleanQuery(BooleanQuery queryCandidate) {
        if (!(queryCandidate instanceof BooleanQuery))
             throw new SolrException(ErrorCode.SERVER_ERROR, "Argument is not a BooleanQuery");
        BooleanQuery.Builder queryBuilder = new BooleanQuery.Builder();
        for (BooleanClause currentClause : queryCandidate.clauses()) {
            Query subquery = currentClause.getQuery();
            if (subquery instanceof TermQuery) {
                subquery = processTermQuery((TermQuery)subquery);
            } else if (subquery instanceof DisjunctionMaxQuery) {
                subquery = processDisjunctionMaxQuery((DisjunctionMaxQuery)subquery);
            } else if (subquery instanceof BoostQuery) {
                subquery = processBoostQuery((BoostQuery)subquery);
            } else if (subquery instanceof BooleanQuery) {
                subquery = processBooleanQuery((BooleanQuery)subquery);
            } else
                logger.warn("No appropriate Query in BooleanClause for " + subquery.getClass().getName());
            queryBuilder.add(subquery, currentClause.getOccur());
       }
       return queryBuilder.build();
    }


    protected Query processTermRangeQuery(TermRangeQuery queryCandidate) {
       if (!(queryCandidate instanceof TermRangeQuery))
           throw new SolrException(ErrorCode.SERVER_ERROR, "Argument is not a TermRangeQuery");
       queryCandidate = queryCandidate;
       String field = queryCandidate.getField();
       String newFieldName = field + "_" + lang;
       if (schema.getFieldOrNull(newFieldName) != null) {
           return new TermRangeQuery(newFieldName,
                                     queryCandidate.getLowerTerm(),
                                     queryCandidate.getUpperTerm(),
                                     queryCandidate.includesLower(),
                                     queryCandidate.includesUpper());
       }
       return queryCandidate;
    }




    protected void handleLuceneParser(String[] query, SolrQueryRequest request, String lang, IndexSchema schema) throws MultiLanguageQueryParserException {
       if (query.length == 1) {
           try {
               QParser tmpParser = new ExtendedDismaxQParser(searchString, localParams, params, request);
               newQuery = tmpParser.getQuery();
               newQuery = newQuery.rewrite(request.getSearcher().getIndexReader());
               if (newQuery instanceof BooleanQuery)
                   newQuery = processBooleanQuery((BooleanQuery)newQuery);
               else if (newQuery instanceof TermRangeQuery)
                   newQuery = processTermRangeQuery((TermRangeQuery)newQuery);
               else if (newQuery instanceof TermQuery)
                   newQuery = processTermQuery((TermQuery)newQuery);
               else if (newQuery instanceof DisjunctionMaxQuery)
                   newQuery = processDisjunctionMaxQuery((DisjunctionMaxQuery)newQuery);
               else if (newQuery instanceof BoostQuery)
                   newQuery = processBoostQuery((BoostQuery)newQuery);
               else
                   logger.warn("No rewrite rule did match for " + newQuery.getClass());
               this.searchString = newQuery.toString();
           } catch(SyntaxError|IOException e) {
               throw new SolrException(ErrorCode.SERVER_ERROR, "Rewriting Lucene support for new languages failed", e);
           }
       } else
           throw new MultiLanguageQueryParserException("Only one q-parameter is supported [1]");
    }


    public Query parse() throws SyntaxError {
        if (newQuery == null) {
           this.newRequest.setParams(newParams);
           QParser parser = getParser(this.searchString, "edismax", this.newRequest);
           return parser.parse();
        }
        return newQuery;
    }
}

