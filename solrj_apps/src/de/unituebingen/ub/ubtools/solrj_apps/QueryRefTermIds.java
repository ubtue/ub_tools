package de.unituebingen.ub.ubtools.solrj_apps;
import org.apache.solr.client.solrj.SolrQuery;
import org.apache.solr.client.solrj.SolrServerException;
import org.apache.solr.client.solrj.impl.HttpSolrClient;
import org.apache.solr.client.solrj.SolrClient;
import org.apache.solr.client.solrj.response.UpdateResponse;
import org.apache.solr.client.solrj.response.QueryResponse;
import org.apache.solr.common.SolrInputDocument;
import org.apache.solr.common.SolrDocumentList;
import org.apache.solr.common.SolrDocument;
import org.apache.solr.client.solrj.util.ClientUtils;
import org.apache.log4j.Logger;
import org.apache.log4j.Level;
import java.util.Map;
import java.util.HashMap;
import java.io.IOException;
import java.io.BufferedWriter;
import java.io.File;
import java.io.Writer;
import java.io.FileWriter;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;


public class QueryRefTermIds {

    private static final String REPLACEMENT_STRING = "Papstreise";

    private static final String url = "http://localhost:8080/solr/biblio";
    private HttpSolrClient solr;

    public QueryRefTermIds() {
        if (solr == null)
            solr = new HttpSolrClient(url);
    }

    public QueryResponse querySolr(String queryString) {
        SolrQuery query = new SolrQuery();
        query.set("q", queryString);
        query.set("fl", "id");
        query.set("rows", Integer.MAX_VALUE);
        QueryResponse response = null;
        try {
            response = solr.query(query);
        } catch (SolrServerException e) {
             e.printStackTrace();
        } catch (IOException e) {
             e.printStackTrace();
        }
        return response;   
    }

    public void augmentTopicField(String id, String valueToAdd) {
       SolrInputDocument doc = new SolrInputDocument();
       Map<String, String> partialUpdate = new HashMap<String, String>();
       partialUpdate.put("set", valueToAdd); 
       doc.addField("id", id);
       doc.addField("topic_hintterms", partialUpdate);
       
       try {
           UpdateResponse response = solr.add(doc);
           solr.commit();
       } catch (SolrServerException e) {
           e.printStackTrace();
       } catch (IOException e) {
          e.printStackTrace();
       }
    }
  
    public void reinsertDocument(SolrInputDocument doc) {
        try {
            UpdateResponse response = solr.add(doc);
            solr.commit();
        } catch (SolrServerException e) {
           e.printStackTrace();
        } catch (IOException e) {
          e.printStackTrace();
        }
    }

    public static void Usage(String[] args) {
        System.out.println("Usage: " +  System.getProperty("sun.java.command") + " output_dir reference_term query_string");
        System.out.println("We got " + args.length + " parameters");
        for (String arg : args) {
             System.out.println("ARG:" + arg);

        }
    }

    public static void main(String[] args) {

        if (args.length != 3){
            Usage(args);
            System.exit(1);
        }      

        Logger.getRootLogger().setLevel(Level.OFF);

        final String outputDir = args[0];
        final String referenceTerm = args[1];
        final String queryString = args[2];
        QueryRefTermIds queryRefTermIds = new QueryRefTermIds();
        QueryResponse response = queryRefTermIds.querySolr(queryString);
        SolrDocumentList docList = response.getResults();

  
        if ( docList.size() != 0) {
            try {
       	        new File(outputDir).mkdirs();
                String outfileName = referenceTerm.replaceAll("^\"|\"$", "");         
                Writer writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputDir + "/" + outfileName), "utf-8")); 
                System.err.println("Writing file " + outfileName);
                for (SolrDocument doc : docList) {
                    String id = (String)doc.getFieldValue("id");
                    writer.write(id + "|" + referenceTerm + '\n');
                }
                writer.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
};
