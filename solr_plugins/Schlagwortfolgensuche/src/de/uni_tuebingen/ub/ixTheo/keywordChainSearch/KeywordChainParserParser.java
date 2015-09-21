package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;


import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;
import java.util.ArrayList;
import java.util.Arrays;


public class KeywordChainParserParser extends QParser {

    private Query innerQuery;
    private String origQueryString;

    public KeywordChainParserParser(final String searchString, final SolrParams localParams, final SolrParams params, final SolrQueryRequest request) {
        super(searchString, localParams, params, request);
        try {
        	this.origQueryString = searchString;
           	final String queryString = "key_word_chain_bag:(" + adjustQueryString(origQueryString) +")";
	    
        	//String reqString = getReq().getParamString();
	    
        	final QParser parser = getParser(queryString, "lucene", getReq());
        	this.innerQuery = parser.parse();
        } 
        catch (SyntaxError ex) {
            throw new RuntimeException("error parsing query", ex);
        }
    }


    public Query parse() throws SyntaxError {
    	// return this.innerQuery;
    	return new KeywordChainSearchQuery(innerQuery, origQueryString);
    }
    
    
    public String adjustQueryString(String origQueryString) {
    	
    	// We would like to flexible as to the matches in the key_word_chain_bag to acquire 
    	// a broad set for the Scorer to compare on, so truncate the terms of the original search
    	// for the lucene search
    	
    	private final static int PREFIX_LENGTH = 4;
    	
    	origQueryString = origQueryString.replaceAll("[^\\p{L}\\p{Nd}\\s]+", "");
	    ArrayList<String> queryList = new ArrayList<String>(Arrays.asList(origQueryString.split("[\\s]")));
	    
	    String newQueryString = "";
	    
	    for (String queryTerm : queryList) {
	    	
	    	if (queryTerm.length() == 0)
	    		continue;
	    	
	    	// trunk down to the beginning letters
	    	if (queryTerm.length() > PREFIX_LENGTH) {
	    		queryTerm = queryTerm.substring(0, PREFIX_LENGTH);
	    	}
	    	
	    	newQueryString += queryTerm + "*" + " ";
   	
	    }
    	
	    return  newQueryString;

    }

}



