package de.uni_tuebingen.ub.ixTheo.multiLanguageQuery;

import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.params.ModifiableSolrParams; 
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.apache.commons.lang.StringUtils;
import org.apache.commons.lang.ArrayUtils;


public class MultiLanguageQueryParser extends QParser {

    protected Query query;
    protected String searchString;
    protected static Logger logger = LoggerFactory
        .getLogger(MultiLanguageQueryParser.class);

    protected String[] SUPPORTED_LANGUAGES = { "de", "en", "fr" };

    public MultiLanguageQueryParser(final String searchString, final SolrParams localParams, final SolrParams params, final SolrQueryRequest request) {

       super(searchString, localParams, params, request);
       this.searchString = searchString;
       
       ModifiableSolrParams newParams = new ModifiableSolrParams();
       newParams.add(params);
       
//       System.out.println(params.toString());
logger.info("WE HAVE GOT MultiLanguageQueryParser with params: "  + params.toString());
        String[] queryFields = newParams.getParams("qf");
        String[] facetFields = newParams.getParams("facet.field");
        String lang = newParams.get("lang", "de");

        // Strip language subcode
        lang = lang.split("-")[0];

        // Set default language if we do not directly support the chosen language

        lang = ArrayUtils.contains(SUPPORTED_LANGUAGES, lang) ? lang : "de";


        for (String param : queryFields) {
            newParams.remove("qf", param);
            newParams.add("qf", param + "_" + lang);
        } 


        for (String param : facetFields) {
            newParams.remove("facet.field", param);
            newParams.add("facet.field", param + "_" + lang);
        }


logger.info("MultiLanguageQueryParser REWRITE:" + newParams.toString());

       request.setParams(newParams);

    }

//String[] fqs = req.getParams().getParams(CommonParams.FQ);

    public Query parse() throws SyntaxError {
        final QParser parser = getParser(this.searchString, "lucene", getReq());
        return parser.parse();
       
    }

}
