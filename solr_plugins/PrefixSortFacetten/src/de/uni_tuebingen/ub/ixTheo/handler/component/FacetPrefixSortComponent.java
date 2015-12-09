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

//package org.apache.solr.handler.component;
package de.uni_tuebingen.ub.ixTheo.handler.component;

import java.io.IOException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.TreeMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Collection;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

import org.apache.lucene.util.FixedBitSet;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.SolrException.ErrorCode;
import org.apache.solr.common.params.CommonParams;
import org.apache.solr.common.params.FacetParams;
import org.apache.solr.common.params.ModifiableSolrParams;
import org.apache.solr.common.params.ShardParams;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.SimpleOrderedMap;
import org.apache.solr.common.util.StrUtils;
import org.apache.solr.request.SimpleFacets;
import org.apache.solr.schema.FieldType;
import org.apache.solr.search.QueryParsing;
import org.apache.solr.search.SyntaxError;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import org.apache.solr.handler.component.*;
import de.uni_tuebingen.ub.ixTheo.common.params.*;
import de.uni_tuebingen.ub.ixTheo.common.util.*;

/**
 * TODO!
 *
 *
 * @since solr 1.3
 */
@SuppressWarnings("rawtypes")
// public class FacetComponent extends SearchComponent {
public class FacetPrefixSortComponent extends FacetComponent {

    // Redefined for finishStage() since private in FacetComponent
    int pivotRefinementCounter = 0;
    private static final String PIVOT_KEY = "facet_pivot";

    @Override
    public void prepare(ResponseBuilder rb) throws IOException {

        SolrParams params = rb.req.getParams();
        super.prepare(rb);

    }

    /**
     * Actually run the query
     */
    @Override
    public void process(ResponseBuilder rb) throws IOException {

        if (rb.doFacets) {
            ModifiableSolrParams params = new ModifiableSolrParams();
            SolrParams origParams = rb.req.getParams();
            Iterator<String> iter = origParams.getParameterNamesIterator();
            while (iter.hasNext()) {
                String paramName = iter.next();
                // Deduplicate the list with LinkedHashSet, but _only_ for facet
                // params.
                if (paramName.startsWith(FacetParams.FACET) == false) {
                    params.add(paramName, origParams.getParams(paramName));
                    continue;
                }
                HashSet<String> deDupe = new LinkedHashSet<>(Arrays.asList(origParams.getParams(paramName)));
                params.add(paramName, deDupe.toArray(new String[deDupe.size()]));
            }

            SimpleFacets f = new SimpleFacets(rb.req, rb.getResults().docSet, params, rb);

            NamedList<Object> counts = f.getFacetCounts();
            String[] pivots = params.getParams(FacetParams.FACET_PIVOT);
            if (pivots != null && pivots.length > 0) {
                PivotFacetProcessor pivotProcessor = new PivotFacetProcessor(rb.req, rb.getResults().docSet, params,
                        rb);
                SimpleOrderedMap<List<NamedList<Object>>> v = pivotProcessor.process(pivots);
                if (v != null) {
                    counts.add(PIVOT_KEY, v);
                }
            }

            // Check whether we have to reorder out results
            // according to prefix

            String sort = params.get(FacetParams.FACET_SORT);
            if (FacetPrefixSortParams.FACET_SORT_PREFIX.equals(sort)) {

                // Determine a score relative to the original query

                // Determine the query and make it compatible with our metric
                // class
                // by splitting the single terms
                String[] queryTerms = params.getParams(CommonParams.Q);
                Collection<String> queryTermsCollection = new ArrayList<String>();

                // We should split at spaces but pay attention to quoted strings
                // which
                // can have internally escaped quotes again. The regex is based
                // on
                // http://www.metaltoad.com/blog/regex-quoted-string-escapable-quotes
                // (2015-12-2)
                // to achieve this.

                String pattern = "((?<![\\\\])['\"]|\\\\\")((?:.(?!(?<![\\\\])\\1))*.?)\\1|([^\\s]+)";
                Pattern regex = Pattern.compile(pattern);

                for (String s : queryTerms) {

                    // Split at whitespace except we have a quoted term

                    Matcher matcher = regex.matcher(s);

                    while (matcher.find()) {

                        queryTermsCollection.add(matcher.group().replaceAll("^\"|\"$", ""));

                    }

                }

                queryTerms = queryTermsCollection.toArray(new String[] {});

                ArrayList<String> queryList = new ArrayList<String>(Arrays.asList(queryTerms));
                String facetfield = params.get(FacetParams.FACET_FIELD);

                // Get the current facet entry and make it compatible with our
                // metric class
                // "facet_fields" itself contains a NamedList with the
                // facet.field as key

                NamedList<Object> facetFieldsNamedList = (NamedList<Object>) counts.get("facet_fields");
                NamedList<Object> facetFields = (NamedList<Object>) facetFieldsNamedList.get(facetfield);

                Iterator<Map.Entry<String, Object>> iterFacets = facetFields.iterator();

                Map<Map.Entry<String, Object>, Double> facetMapPrefixScored = new HashMap<Map.Entry<String, Object>, Double>();

                while (iterFacets.hasNext()) {

                    Map.Entry<String, Object> entry = iterFacets.next();

                    String facetTerms = entry.getKey();

                    // Split up each KWC and calculate the scoring

                    ArrayList<String> facetList = new ArrayList<String>(Arrays.asList(facetTerms.split("/")));
                    Double score = (Double) KeywordChainMetric.calculateSimilarityScore(queryList, facetList);
                    // Collect the result in a sorted list

                    facetMapPrefixScored.put(entry, score);

                }

                @SuppressWarnings("unchecked")
                Entry<Entry<String, Object>, Double>[] facetPrefixArrayScored = facetMapPrefixScored.entrySet()
                        .toArray(new Entry[facetMapPrefixScored.size()]);

                Arrays.sort(facetPrefixArrayScored, new Comparator<Entry<Entry<String, Object>, Double>>() {

                    public int compare(Entry<Entry<String, Object>, Double> e1,
                            Entry<Entry<String, Object>, Double> e2) {

                        // We would like to score according to the second
                        // element
                        return e2.getValue().compareTo(e1.getValue());
                    }
                });

                // Extract all the values wrap it back to NamedList again and
                // replace in the original structure

                facetFieldsNamedList.clear();

                NamedList<Object> facetNamedListSorted = new NamedList<Object>();

                for (Entry<Entry<String, Object>, Double> e : facetPrefixArrayScored) {
                    // facetNamedListSorted.add(e.getKey().getKey(),
                    // e.getKey().getValue().toString() + ":" +
                    // e.getValue().toString() );
                    facetNamedListSorted.add(e.getKey().getKey(), e.getKey().getValue());
                }

                facetFieldsNamedList.add(facetfield, facetNamedListSorted);

                counts.remove("facet_fields");
                counts.add("facet_fields", facetFieldsNamedList);

            }

            rb.rsp.add("facet_counts", counts);
        }

    }

    private static final String commandPrefix = "{!" + CommonParams.TERMS + "=$";

    @Override
    public void handleResponses(ResponseBuilder rb, ShardRequest sreq) {
        super.handleResponses(rb, sreq);
    }

    @Override
    public void finishStage(ResponseBuilder rb) {
        pivotRefinementCounter = 0;
        if (!rb.doFacets || rb.stage != ResponseBuilder.STAGE_GET_FIELDS)
            return;
        // wait until STAGE_GET_FIELDS
        // so that "result" is already stored in the response (for aesthetics)

        FacetInfo fi = rb._facetInfo;

        NamedList<Object> facet_counts = new SimpleOrderedMap<>();

        NamedList<Number> facet_queries = new SimpleOrderedMap<>();
        facet_counts.add("facet_queries", facet_queries);
        for (QueryFacet qf : fi.queryFacets.values()) {
            facet_queries.add(qf.getKey(), num(qf.count));
        }

        NamedList<Object> facet_fields = new SimpleOrderedMap<>();
        facet_counts.add("facet_fields", facet_fields);

        for (DistribFieldFacet dff : fi.facets.values()) {
            // order is important for facet values, so use NamedList
            NamedList<Object> fieldCounts = new NamedList<>();
            facet_fields.add(dff.getKey(), fieldCounts);

            ShardFacetCount[] counts;
            boolean countSorted = dff.sort.equals(FacetParams.FACET_SORT_COUNT);
            if (countSorted) {
                counts = dff.countSorted;
                if (counts == null || dff.needRefinements) {
                    counts = dff.getCountSorted();
                }
            } else if (dff.sort.equals(FacetParams.FACET_SORT_INDEX)) {
                counts = dff.getLexSorted();
            } else { // TODO: log error or throw exception?
                counts = dff.getLexSorted();
            }

            if (countSorted) {
                int end = dff.limit < 0 ? counts.length : Math.min(dff.offset + dff.limit, counts.length);
                for (int i = dff.offset; i < end; i++) {
                    if (counts[i].count < dff.minCount) {
                        break;
                    }
                    fieldCounts.add(counts[i].name, num(counts[i].count));
                }
            } else {
                int off = dff.offset;
                int lim = dff.limit >= 0 ? dff.limit : Integer.MAX_VALUE;

                // index order...
                for (int i = 0; i < counts.length; i++) {
                    long count = counts[i].count;
                    if (count < dff.minCount)
                        continue;
                    if (off > 0) {
                        off--;
                        continue;
                    }
                    if (lim <= 0) {
                        break;
                    }
                    lim--;
                    fieldCounts.add(counts[i].name, num(count));
                }
            }

            if (dff.missing) {
                fieldCounts.add(null, num(dff.missingCount));
            }
        }

        facet_counts.add("facet_dates", fi.dateFacets);
        facet_counts.add("facet_ranges", fi.rangeFacets);
        facet_counts.add("facet_intervals", fi.intervalFacets);

        if (fi.pivotFacets != null && fi.pivotFacets.size() > 0) {
            facet_counts.add(PIVOT_KEY, createPivotFacetOutput(rb));
        }

        rb.rsp.add("facet_counts", facet_counts);

        rb._facetInfo = null; // could be big, so release asap

    }

    // Helper functions that must be redefined since they are private in Facet
    // Component

    private SimpleOrderedMap<List<NamedList<Object>>> createPivotFacetOutput(ResponseBuilder rb) {

        SimpleOrderedMap<List<NamedList<Object>>> combinedPivotFacets = new SimpleOrderedMap<>();
        for (Entry<String, PivotFacet> entry : rb._facetInfo.pivotFacets) {
            String key = entry.getKey();
            PivotFacet pivot = entry.getValue();
            List<NamedList<Object>> trimmedPivots = pivot.getTrimmedPivotsAsListOfNamedLists(rb);
            if (null == trimmedPivots) {
                trimmedPivots = Collections.<NamedList<Object>> emptyList();
            }

            combinedPivotFacets.add(key, trimmedPivots);
        }
        return combinedPivotFacets;
    }

    private Number num(long val) {
        if (val < Integer.MAX_VALUE)
            return (int) val;
        else
            return val;
    }

    private Number num(Long val) {
        if (val.longValue() < Integer.MAX_VALUE)
            return val.intValue();
        else
            return val;
    }

    /////////////////////////////////////////////
    /// SolrInfoMBean
    ////////////////////////////////////////////

    @Override
    public String getDescription() {
        return "Handle Prefix Sort Faceting";
    }

    @Override
    public String getSource() {
        return null;
    }

    @Override
    public URL[] getDocs() {
        return null;
    }

}
