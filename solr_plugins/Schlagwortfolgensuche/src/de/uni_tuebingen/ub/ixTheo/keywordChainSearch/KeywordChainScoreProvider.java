package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import org.apache.lucene.document.Document;
import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.search.Query;


public class KeywordChainScoreProvider extends CustomScoreProvider {

        String queryString;
        String origQueryString;
        Query query;

        public KeywordChainScoreProvider(final AtomicReaderContext context,
					   final Query query, final String origQueryString) {
                super(context);
                this.origQueryString = origQueryString;
                this.query = query;
                this.queryString = query.toString("key_word_chain_bag");
        }

		@Override
        public float customScore(final int docId, final float subQueryScore, final float valSrcScores[]) throws IOException {

	    // We want the query be represented such that we can compare it to the
	    // key_word_chains. To make it compatible to Solr Keyword chains are stored
	    // as "/"-separated Strings
	    // We need to convert them to ArrayListy<String> and then do the comparison for
	    // each keyword chain entry

        final int MAXARRAYSIZE = 20;	
        	
		ArrayList<String> keywordChainValues = new ArrayList<String>();
	    ArrayList<String>[] keywordChainStringsList = (ArrayList<String>[])new ArrayList[MAXARRAYSIZE];
	    ArrayList<Double> documentScoringResults = new ArrayList<Double>();
	    	    
	    queryString = origQueryString.replaceAll("[^\\p{L}\\p{Nd}\\s]+", "");
	    ArrayList<String> queryList = new ArrayList<String>(Arrays.asList(queryString.split("[\\s]")));
	    // Replace possible non word characters
	    
	    
	    Document d = context.reader().document(docId);
	    // plugin external score calculation based on the fields...

	    // 1.) Get all "/"-separated keywordchains 
	    keywordChainValues.addAll(Arrays.asList(d.getValues("key_word_chains")));

	    // 2.) Split up the keywordChain
	    int i = 0;
	 
	    for (String keywordChainStringComposite : keywordChainValues) {
	    	String[] keywordChainStringSplit = keywordChainStringComposite.split("/");
	    	keywordChainStringsList[i++] = new ArrayList<String>(Arrays.asList(keywordChainStringSplit));
	    }

	    // 3.) Compare the KWC	
	    for (ArrayList<String> keywordChain : keywordChainStringsList){
	    	
	    	KeywordChainMetric keywordChainMetric = new KeywordChainMetric();

	    	if (keywordChain == null)
	    		continue;

	    	if ((! keywordChain.isEmpty())  &&  (! queryList.isEmpty()))
	    		documentScoringResults.add(keywordChainMetric.calculateSimilarityScore(queryList, keywordChain));
	    	}

	    	// and return the custom score

	    	float score = Collections.max(documentScoringResults).floatValue();
	    	return score;
        }

    
}




