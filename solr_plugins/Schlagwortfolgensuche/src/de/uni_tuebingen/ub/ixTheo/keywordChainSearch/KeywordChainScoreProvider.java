package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;

import java.io.IOException;
import java.util.List;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;



import org.apache.lucene.document.Document;
import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.index.IndexReader;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.document.Document;

public class KeywordChainScoreProvider extends CustomScoreProvider {

        String queryString;

        public KeywordChainScoreProvider(final AtomicReaderContext context,
					   final String queryString) {
                super(context);
		this.queryString = queryString;
        }

        @Override
        public float customScore(final int docId, final float subQueryScore, final float valSrcScores[]) throws IOException {

	    // We want the query be represented such that we can compare it to the
	    // key_word_chains. To make it compatible to Solr Keyword chains are stored
	    // as "/"-separated Strings
	    // We need to convert them to ArrayListy<String> and then do the comparison for
	    // each keyword chain entry
	    

	    // TEST ONLY
	    //queryString = "Kirchengeschichte";
	    // END TEST ONLY
        	
         	
        // Test to extract all the Terms from query
        	
//        ArrayList<Term> termList = new ArrayList<Term>();
        
//        Arrays.asList(Query.parse(queryString).extractTerms()))

	    ArrayList<String> queryList = new ArrayList<String>(Arrays.asList(queryString.split("/"))); 
	    ArrayList<String> keywordChainValues = new ArrayList<String>();
	    ArrayList<String>[] keywordChainStringsList = (ArrayList<String>[])new ArrayList[20];
	    ArrayList<Double> documentScoringResults = new ArrayList<Double>();

	    queryList.addAll(Arrays.asList(queryString.split(" ")));

	    Document d = context.reader().document(docId);
	    // plugin external score calculation based on the fields...
	    //List fields = d.getFields();

	    // 1.) Get all "/"-separated keywordchains 
	    keywordChainValues.addAll(Arrays.asList(d.getValues("key_word_chains")));

	    // 2.) Split up the keywordChain
	    int i = 0;
	 
	    for(String keywordChainStringComposite : keywordChainValues){
		String[] keywordChainStringSplit = keywordChainStringComposite.split("/");

	    	keywordChainStringsList[i++] = new ArrayList<String>(Arrays.asList(keywordChainStringSplit));
	    }

	    // 3.) Compare the KWC	
	    for(ArrayList<String> keywordChain : keywordChainStringsList){
		KeywordChainMetric keywordChainMetric = new KeywordChainMetric();

		if(keywordChain == null)
		    continue;

		if((! keywordChain.isEmpty())  &&  (! queryList.isEmpty()))
		    documentScoringResults.add(keywordChainMetric.calculateSimilarityScore(queryList, keywordChain));
	    }

	    // and return the custom score
	    
	    //	    float score = 0.001f;

	    float score = Collections.max(documentScoringResults).floatValue();
	    return score;
	    //...
        }

    
}




