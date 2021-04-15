/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package de.uni_tuebingen.ub.ixTheo.util;

import java.io.IOException;
import java.util.Map;

import org.apache.lucene.search.Explanation;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.CommonParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.SimpleOrderedMap;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.schema.IndexSchema;
import org.apache.solr.search.DocList;
import org.apache.solr.search.SolrIndexSearcher;

/**
 * Utilities that may be of use to RequestHandlers.
 */
public class SolrPluginUtils extends org.apache.solr.util.SolrPluginUtils {

    //private static final Logger log = LoggerFactory.getLogger(MethodHandles.lookup().lookupClass());

    private static NamedList<String> explanationsToStrings(NamedList<Explanation> explanations) {

        NamedList<String> out = new SimpleOrderedMap<>();
        for (Map.Entry<String, Explanation> entry : explanations) {
            out.add(entry.getKey(), "\n" + entry.getValue().toString());
        }
        return out;
    }

    public static NamedList doStandardDebug(SolrQueryRequest req, String userQuery, Query query, DocList results, boolean dbgQuery, boolean dbgResults)
        throws IOException 
    {
        NamedList dbg = new SimpleOrderedMap();
        doStandardQueryDebug(req, userQuery, query, dbgQuery, dbg);
        doStandardResultsDebug(req, query, results, dbgResults, dbg);
        return dbg;
    }

    public static void doStandardResultsDebug(SolrQueryRequest req, Query query, DocList results, boolean dbgResults, NamedList dbg) throws IOException {
        if (dbgResults) {
            SolrIndexSearcher searcher = req.getSearcher();
            IndexSchema schema = searcher.getSchema();
            boolean explainStruct = req.getParams().getBool(CommonParams.EXPLAIN_STRUCT, false);

            if (results != null) {
                NamedList<Explanation> explain = getExplanations(query, results, searcher, schema);
                dbg.add("explain", explainStruct ? explanationsToNamedLists(explain) : explanationsToStrings(explain));
            }

            String otherQueryS = req.getParams().get(CommonParams.EXPLAIN_OTHER);
            if (otherQueryS != null && otherQueryS.length() > 0) {
                DocList otherResults = doSimpleQuery(otherQueryS, req, 0, results.size());
                dbg.add("otherQuery", otherQueryS);
                NamedList<Explanation> explainO = getExplanations(query, otherResults, searcher, schema);
                dbg.add("explainOther", explainStruct ? explanationsToNamedLists(explainO) : explanationsToStrings(explainO));
            }
        }
    }
}
