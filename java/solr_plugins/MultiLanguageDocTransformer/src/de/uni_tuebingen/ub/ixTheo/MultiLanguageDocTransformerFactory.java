// Based on code from http://lifelongprogrammer.blogspot.de/2013/06/solr-use-doctransformer-to-change.html (20160613)

// What we would like to do is extract a translation from a multivalue field

package de.uni_tuebingen.ub.ixTheo;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.solr.common.SolrDocument;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.StrUtils;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.response.transform.DocTransformer;
import org.apache.solr.response.transform.RenameFieldTransformer;
import org.apache.solr.response.transform.TransformerFactory;
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
        return new MultiLanguageDocTransformer(reqLang);
    }

    class MultiLanguageDocTransformer extends DocTransformer {
        protected String reqLang;

        public MultiLanguageDocTransformer(final String reqLang) {
            this.reqLang = reqLang;
        }

        @Override
        public String getName() {
            return MultiLanguageDocTransformer.class.getName();
        }


        @Override
        public boolean needsSolrIndexSearcher() {
            return true;
        }

        @Override
        public void transform(SolrDocument doc, int docid) throws IOException {
            if (enabled) {
                for (String fieldName : fieldNames) {
                    // Select extension according to chosen language
                    String reqLangExtension = "_" + this.reqLang;
                    int i = langExtensions.indexOf(reqLangExtension);
                    String langExtension = (i != -1) ? langExtensions.get(i) : "_de";
                    RenameFieldTransformer renameFieldTransformer =
                        new RenameFieldTransformer(fieldName + langExtension, fieldName, true /*copy*/);
                    renameFieldTransformer.transform(doc, docid);

                }
            }
        }
    }
}
