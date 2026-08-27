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
import org.apache.lucene.search.MatchNoDocsQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.params.CommonParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.SimpleOrderedMap;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.schema.IndexSchema;
import org.apache.solr.search.DocList;
import org.apache.solr.search.QParser;
import org.apache.solr.search.QueryCommand;
import org.apache.solr.search.QueryParsing;
import org.apache.solr.search.QueryResult;
import org.apache.solr.search.SolrIndexSearcher;
import org.apache.solr.search.SyntaxError;

/**
 * Utilities that may be of use to RequestHandlers.
 *
 * This class intentionally avoids deprecated SolrPluginUtils debug helpers
 * such as doSimpleQuery(), doStandardDebug(), and doStandardResultsDebug().
 */
public class SolrPluginUtils extends org.apache.solr.util.SolrPluginUtils {

    /**
     * Custom optional parameter for controlling how many documents are explained
     * for explainOther.
     *
     * Example:
     *   &debug=true&debugQuery=true&explainOther=id:123&explainOther.rows=10
     */
    private static final String EXPLAIN_OTHER_ROWS = "explainOther.rows";

    private static final int DEFAULT_EXPLAIN_OTHER_ROWS = 10;

    private SolrPluginUtils() {
        // Utility class.
    }

    private static NamedList<String> explanationsToStrings(
            NamedList<Explanation> explanations) {

        NamedList<String> out = new SimpleOrderedMap<>();

        for (Map.Entry<String, Explanation> entry : explanations) {
            out.add(entry.getKey(), "\n" + entry.getValue().toString());
        }

        return out;
    }

    public static NamedList<Object> doStandardDebug(SolrQueryRequest req, String userQuery, Query query, DocList results, boolean dbgQuery, boolean dbgResults) throws IOException {
        NamedList<Object> dbg = new SimpleOrderedMap<Object>();
        doStandardQueryDebug(req, userQuery, query, dbgQuery, dbg);
        doStandardResultsDebug(req, query, results, dbgResults, dbg);
        return dbg;
    }

    public static void doStandardQueryDebug(SolrQueryRequest req, String userQuery, Query query, boolean dbgQuery, NamedList<Object> dbg) {

        if (!dbgQuery) {
            return;
        }

        String rawQueryString = req.getParams().get(CommonParams.Q);

        if (rawQueryString != null) {
            dbg.add("rawquerystring", rawQueryString);
        }

        if (userQuery != null) {
            dbg.add("querystring", userQuery);
        }

        if (query != null) {
            dbg.add("parsedquery", QueryParsing.toString(query, req.getSchema()));
            dbg.add("parsedquery_toString", query.toString());
        }
    }

    public static void doStandardResultsDebug(
            SolrQueryRequest req,
            Query mainQuery,
            DocList results,
            boolean dbgResults,
            NamedList<Object> dbg) throws IOException {

        if (!dbgResults) {
            return;
        }

        SolrIndexSearcher searcher = req.getSearcher();
        IndexSchema schema = searcher.getSchema();
        boolean explainStruct = req.getParams()
                .getBool(CommonParams.EXPLAIN_STRUCT, false);

        if (results != null) {
            NamedList<Explanation> explain =
                    org.apache.solr.util.SolrPluginUtils.getExplanations(
                            mainQuery,
                            results,
                            searcher,
                            schema);

            dbg.add("explain", formatExplanations(explain, explainStruct));
        }

        String otherQueryString = req.getParams().get(CommonParams.EXPLAIN_OTHER);

        if (otherQueryString != null && !otherQueryString.trim().isEmpty()) {
            DocList otherResults = doExplainOtherQuery(req, otherQueryString);

            dbg.add("otherQuery", otherQueryString);

            /*
             * Important:
             * The documents come from explainOther, but the explanations are
             * calculated against the main query. This matches Solr's semantics.
             */
            NamedList<Explanation> explainOther =
                    org.apache.solr.util.SolrPluginUtils.getExplanations(
                            mainQuery,
                            otherResults,
                            searcher,
                            schema);

            dbg.add("explainOther",
                    formatExplanations(explainOther, explainStruct));
        }
    }

    private static Object formatExplanations(
            NamedList<Explanation> explanations,
            boolean explainStruct) {

        if (explainStruct) {
            return org.apache.solr.util.SolrPluginUtils
                    .explanationsToNamedLists(explanations);
        }

        return explanationsToStrings(explanations);
    }

    private static DocList doExplainOtherQuery(
            SolrQueryRequest req,
            String otherQueryString) throws IOException {

        Query otherQuery = parseExplainOtherQuery(req, otherQueryString);
        SolrIndexSearcher searcher = req.getSearcher();

        int rows = getExplainOtherRows(req);

        QueryCommand command = new QueryCommand()
                .setQuery(otherQuery)
                .setOffset(0)
                .setLen(rows)
                .setFlags(SolrIndexSearcher.GET_DOCLIST);

        QueryResult result = searcher.search(command);

        return result.getDocList();
    }

    private static Query parseExplainOtherQuery(
            SolrQueryRequest req,
            String otherQueryString) {

        try {
            Query parsedQuery = QParser
                    .getParser(otherQueryString, req)
                    .getQuery();

            if (parsedQuery == null) {
                return new MatchNoDocsQuery();
            }

            return parsedQuery;
        } catch (SyntaxError e) {
            throw new SolrException(
                    SolrException.ErrorCode.BAD_REQUEST,
                    "Invalid " + CommonParams.EXPLAIN_OTHER
                            + " query: " + otherQueryString,
                    e);
        }
    }

    private static int getExplainOtherRows(SolrQueryRequest req) {
        int rows = req.getParams().getInt(
                EXPLAIN_OTHER_ROWS,
                req.getParams().getInt(
                        CommonParams.ROWS,
                        DEFAULT_EXPLAIN_OTHER_ROWS));

        /*
         * Do not allow rows=0 to suppress explainOther.
         * The old code effectively did this through results.size().
         */
        if (rows <= 0) {
            return DEFAULT_EXPLAIN_OTHER_ROWS;
        }

        return rows;
    }
}