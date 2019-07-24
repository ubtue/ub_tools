package de.uni_tuebingen.ub.ixTheo.conditionalInsertField;

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
        String fulltext = (String) doc.getFieldValue("fulltext");
        if (fulltext != null && !fulltext.isEmpty())
            doc.addField("has_fulltext", "true");
    }
}
