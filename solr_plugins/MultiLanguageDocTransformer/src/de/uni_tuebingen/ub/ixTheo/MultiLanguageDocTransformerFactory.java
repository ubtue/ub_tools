// Based on code from http://lifelongprogrammer.blogspot.de/2013/06/solr-use-doctransformer-to-change.html (20160613)

// What we would like to do is extract a translation from a multivalue field

package de.uni_tuebingen.ub.ixTheo;

import java.io.IOException;
import java.util.List;
import java.util.ArrayList;
import org.apache.lucene.document.Field;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.StrUtils;
import org.apache.solr.common.SolrDocument;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.SolrException.ErrorCode;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.response.transform.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MultiLanguageDocTransformerFactory extends TransformerFactory {
    private List<String> fieldNames = new ArrayList<String>();
    private List<String> langExtensions = new ArrayList<String>();
    private boolean enabled = false;
    private String reqLang = "de";
    protected static Logger logger = LoggerFactory.getLogger(MultiLanguageDocTransformerFactory.class);

    @SuppressWarnings("rawtypes")
    @Override
    public void init(NamedList args) {
        super.init(args);
        if (args != null) {
            SolrParams params = SolrParams.toSolrParams(args);
            enabled = params.getBool("enabled", false);
            String str = params.get("fields");
            if (str != null) {
                fieldNames = StrUtils.splitSmart(str, ',');
            }
            String langexts = params.get("lang_extensions");
            if (langexts != null) {
                langExtensions = StrUtils.splitSmart(langexts, ',');
            }

        }
    }

    @Override
    public DocTransformer create(String field, SolrParams params, SolrQueryRequest req) {
        reqLang = req.getParams().get("lang", "de");
        // Throw away language subcode
        final String subcodeSeparator = "-";
        reqLang = reqLang.contains(subcodeSeparator) ? reqLang.split(subcodeSeparator)[0] : reqLang;
        return new MultiLanguageDocTransformer();
    }

    class MultiLanguageDocTransformer extends DocTransformer {
        @Override
        public String getName() {
            return MultiLanguageDocTransformer.class.getName();
        }

        @Override
        public void transform(SolrDocument doc, int docid, float score) throws IOException {
            if (enabled) {
                for (String fieldName : fieldNames) {
                    // Select extension according to chosen language
                    String reqLangExtension = "_" + reqLang;
                    int i;
                    String langExtension = ((i = langExtensions.indexOf(reqLangExtension)) != -1)
                            ? langExtensions.get(i) : "_de";

                    Object srcObj = doc.get(fieldName + langExtension);
                    if (srcObj != null)
                        doc.setField(fieldName, srcObj);
                }
            }
        }
    }
}
