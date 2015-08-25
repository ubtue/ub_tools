package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;

import java.io.IOException;
import java.util.List;

import org.apache.lucene.document.Document;
import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.index.IndexReader;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.document.Document;

public class KeywordChainScoreProvider extends CustomScoreProvider {

        public KeywordChainScoreProvider(final AtomicReaderContext context) {
                super(context);
        }

        @Override
        public float customScore(final int docId, final float subQueryScore, final float valSrcScores[]) throws IOException {

	    Document d = context.reader().document(docId);
	    // plugin external score calculation based on the fields...
	    List fields = d.getFields();
	    // and return the custom score
	    float score = 0.001f;
	    return score;
	    //...
        }

    
}




