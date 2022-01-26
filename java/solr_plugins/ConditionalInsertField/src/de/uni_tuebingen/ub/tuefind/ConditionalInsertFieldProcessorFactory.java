package de.uni_tuebingen.ub.tuefind.conditionalInsertField;

import java.io.IOException;

import org.apache.solr.common.SolrInputDocument;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.response.SolrQueryResponse;
import org.apache.solr.update.AddUpdateCommand;
import org.apache.solr.update.processor.UpdateRequestProcessor;
import org.apache.solr.update.processor.UpdateRequestProcessorFactory;

public class ConditionalInsertFieldProcessorFactory extends UpdateRequestProcessorFactory
{
    @Override
    public UpdateRequestProcessor getInstance(SolrQueryRequest req, SolrQueryResponse rsp, UpdateRequestProcessor next) {
         return new ConditionalInsertProcessor(next);
    }
}

class ConditionalInsertProcessor extends UpdateRequestProcessor
{
    public ConditionalInsertProcessor(UpdateRequestProcessor next) {
        super(next);
    }

    @Override
    public void processAdd(AddUpdateCommand cmd) throws IOException {
        addHasFulltext(cmd);
        // pass it up the chain
        super.processAdd(cmd);
    }

    // Add has_fulltext flag if fulltext is present
    protected void addHasFulltext(AddUpdateCommand cmd) throws IOException {
        SolrInputDocument doc = cmd.getSolrInputDocument();
        boolean fulltext_exists = doc.getFieldValue("fulltext") != null &&
                                  !(((String)doc.getFieldValue("fulltext")).isEmpty());
        boolean fulltext_toc_exists = doc.getFieldValue("fulltext_toc") != null &&
                                      !(((String)doc.getFieldValue("fulltext_toc")).isEmpty());
        boolean fulltext_abstract_exists = doc.getFieldValue("fulltext_abstract") != null &&
                                           !(((String)doc.getFieldValue("fulltext_abstract")).isEmpty());
        boolean fulltext_summary_exists = doc.getFieldValue("fulltext_summary") != null &&
                                          !(((String)doc.getFieldValue("fulltext_summary")).isEmpty());
        if (fulltext_exists || fulltext_toc_exists || fulltext_abstract_exists || fulltext_summary_exists)
            doc.addField("has_fulltext", "true");
        else
            doc.addField("has_fulltext", false);

    }
}
