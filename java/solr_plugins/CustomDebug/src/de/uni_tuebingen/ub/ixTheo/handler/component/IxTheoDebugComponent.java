package de.uni_tuebingen.ub.ixTheo.handler.component;

import static org.apache.solr.common.params.CommonParams.FQ;
import static org.apache.solr.common.params.CommonParams.JSON;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.lucene.search.Query;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.handler.component.DebugComponent;
import org.apache.solr.handler.component.ResponseBuilder;
import org.apache.solr.search.DocList;
import org.apache.solr.search.QueryParsing;
import org.apache.solr.search.facet.FacetDebugInfo;

import de.uni_tuebingen.ub.ixTheo.util.SolrPluginUtils;

public class IxTheoDebugComponent extends DebugComponent {
    //private static Logger logger = LoggerFactory.getLogger(IxTheoDebugComponent.class);
    public static final String COMPONENT_NAME = "debug";

    @SuppressWarnings("unchecked")
    @Override
    public void process(ResponseBuilder rb) throws IOException {
        if (rb.isDebug()) {
            DocList results = null;
            // some internal grouping requests won't have results value set
            if (rb.getResults() != null) {
                results = rb.getResults().docList;
            }

            NamedList stdinfo = SolrPluginUtils.doStandardDebug(rb.req, rb.getQueryString(), rb.wrap(rb.getQuery()), results, rb.isDebugQuery(),
                                rb.isDebugResults());

            NamedList info = rb.getDebugInfo();
            if (info == null) {
                rb.setDebugInfo(stdinfo);
                info = stdinfo;
            } else {
                info.addAll(stdinfo);
            }

            FacetDebugInfo fdebug = (FacetDebugInfo) (rb.req.getContext().get("FacetDebugInfo"));
            if (fdebug != null) {
                info.add("facet-trace", fdebug.getFacetDebugInfo());
            }

            fdebug = (FacetDebugInfo) (rb.req.getContext().get("FacetDebugInfo-nonJson"));
            if (fdebug != null) {
                info.add("facet-debug", fdebug.getFacetDebugInfo());
            }

            if (rb.req.getJSON() != null) {
                info.add(JSON, rb.req.getJSON());
            }

            if (rb.isDebugQuery() && rb.getQparser() != null) {
                rb.getQparser().addDebugInfo(rb.getDebugInfo());
            }

            if (null != rb.getDebugInfo()) {
                if (rb.isDebugQuery() && null != rb.getFilters()) {
                    info.add("filter_queries", rb.req.getParams().getParams(FQ));
                    List<String> fqs = new ArrayList<>(rb.getFilters().size());
                    for (Query fq : rb.getFilters()) {
                        fqs.add(QueryParsing.toString(fq, rb.req.getSchema()));
                    }
                    info.add("parsed_filter_queries", fqs);
                }

                // Add this directly here?
                rb.rsp.add("debug", rb.getDebugInfo());
            }
        }
    }
}
