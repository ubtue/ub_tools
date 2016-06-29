package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.search.Query;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;


public class KeywordChainScoreProvider extends CustomScoreProvider {

    String queryString;
    String origQueryString;
    Query query;

    public KeywordChainScoreProvider(final LeafReaderContext context,
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

        ArrayList<String> keywordChainValues = new ArrayList<String>();
        ArrayList<Double> documentScoringResults = new ArrayList<Double>();

        //Replace possible non word characters
        queryString = origQueryString.replaceAll("[^\\p{L}\\p{Nd}\\s]+", "");
        ArrayList<String> queryList = new ArrayList<String>(Arrays.asList(queryString.split("[\\s]")));

        Document d = context.reader().document(docId);

        // Get all "/"-separated keywordchains
        keywordChainValues.addAll(Arrays.asList(d.getValues("key_word_chains")));

        // 2.) Split up the keywordChain and compare the KWC

        for (String keywordChainStringComposite : keywordChainValues) {
            String[] keywordChainStringSplit = keywordChainStringComposite.split("/");
            ArrayList<String> keywordChain = new ArrayList<String>(Arrays.asList(keywordChainStringSplit));

            if ((!keywordChain.isEmpty()) && (!queryList.isEmpty())) {
                documentScoringResults.add(KeywordChainMetric.calculateSimilarityScore(queryList, keywordChain));
            }
        }

        // return the custom score
        return Collections.max(documentScoringResults).floatValue();
    }
}




