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

import de.uni_tuebingen.ub.ixTheo.common.params.FacetPrefixSortParams;
import de.uni_tuebingen.ub.ixTheo.common.util.KeywordChainMetric;
import de.uni_tuebingen.ub.ixTheo.common.util.KeywordSort;
import org.apache.solr.request.SimplePrefixSortFacets;
import org.apache.solr.common.params.CommonParams;
import org.apache.solr.common.params.FacetParams;
import org.apache.solr.common.params.ModifiableSolrParams;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.SimpleOrderedMap;
import org.apache.solr.handler.component.*;
import org.apache.commons.lang.LocaleUtils;
import org.apache.commons.lang.*;

import java.io.IOException;
import java.net.URL;
import java.util.*;
import java.util.Map.Entry;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.AbstractMap;
import java.util.Locale;
import java.text.Collator;

/**
 * TODO!
 *
 * @since solr 1.3
 */
@SuppressWarnings("rawtypes")
// public class FacetComponent extends SearchComponent {
public class FacetPrefixSortComponent extends org.apache.solr.handler.component.FacetComponent {

    // We should split at spaces but pay attention to quoted strings which
    // can have internally escaped quotes again. The regex is based on
    // http://www.metaltoad.com/blog/regex-quoted-string-escapable-quotes
    // (2015-12-2) to achieve this.
    private final static Pattern WHITE_SPACES_WITH_QUOTES_SPLITTING_PATTERN = Pattern
            .compile("((?<![\\\\])['\"]|\\\\\")((?:.(?!(?<![\\\\])\\1))*.?)\\1|([^\\s]+)");

    private final static Pattern LANG_CODE_TRANSFORMATION_PATTERN = Pattern.compile("([a-zA-Z]{2})(-)?([a-zA-Z]{2})?");

    private Collator collator;

    private final Comparator<Entry<Entry<String, Object>, Double>> ENTRY_COMPARATOR = new Comparator<Entry<Entry<String, Object>, Double>>() {
        public int compare(Entry<Entry<String, Object>, Double> e1, Entry<Entry<String, Object>, Double> e2) {

            // We would like to score according to the second element
            int compval = e2.getValue().compareTo(e1.getValue());

            if (compval != 0) {
                return compval;
            } else {
                // Sort according to the chosen language
                return collator.compare(e1.getKey().getKey(), e2.getKey().getKey());
            }
        }
    };

    private static final String PIVOT_KEY = "facet_pivot";

    /**
     * Choose the collator according to the selected language
     */

    private void setCollator(final String langCode) {

        Locale locale = Locale.GERMAN;
        String transformedLangCode = "";

        // Rewrite lang parameter to required layout
        Matcher m = LANG_CODE_TRANSFORMATION_PATTERN.matcher(langCode);
        StringBuffer sb = new StringBuffer(langCode.length());
        while (m.find()) {
            if (m.group(1) != null)
                sb.append(m.group(1).toLowerCase());

            if (m.group(2) != null)
                sb.append(m.group(2).equals("-") ? "_" : "");

            if (m.group(3) != null)
                sb.append(m.group(3).toUpperCase());

            transformedLangCode = sb.toString();
        }

        try {
            locale = LocaleUtils.toLocale(transformedLangCode);
        } catch (IllegalArgumentException e) {
        }

        if (LocaleUtils.isAvailableLocale(locale))
            collator = Collator.getInstance(locale);
        else
            collator = Collator.getInstance(Locale.GERMAN);
    };

    /**
     * Actually run the query
     */
    @Override
    public void process(ResponseBuilder rb) throws IOException {
        if (rb.doFacets) {
            final ModifiableSolrParams params = new ModifiableSolrParams();
            final SolrParams origParams = rb.req.getParams();
            final Iterator<String> iter = origParams.getParameterNamesIterator();
            setCollator(origParams.get("lang"));
            while (iter.hasNext()) {
                final String paramName = iter.next();
                // Deduplicate the list with LinkedHashSet, but _only_ for facet
                // params.
                if (!paramName.startsWith(FacetParams.FACET)) {
                    params.add(paramName, origParams.getParams(paramName));
                    continue;
                }
                final HashSet<String> deDupe = new LinkedHashSet<>(Arrays.asList(origParams.getParams(paramName)));
                params.add(paramName, deDupe.toArray(new String[deDupe.size()]));
            }

            final SimplePrefixSortFacets facets = new SimplePrefixSortFacets(rb.req, rb.getResults().docSet, params, rb);
            final NamedList<Object> counts = org.apache.solr.handler.component.FacetComponent.getFacetCounts(facets);

            final String[] pivots = params.getParams(FacetParams.FACET_PIVOT);
            if (pivots != null && pivots.length > 0) {
                PivotFacetProcessor pivotProcessor = new PivotFacetProcessor(rb.req, rb.getResults().docSet, params, rb);
                SimpleOrderedMap<List<NamedList<Object>>> v = pivotProcessor.process(pivots);
                if (v != null) {
                    counts.add(PIVOT_KEY, v);
                }
            }

            // Check whether we have to reorder out results
            // according to prefix

            final String sort = params.get(FacetParams.FACET_SORT);
            if (FacetPrefixSortParams.FACET_SORT_PREFIX.equals(sort)) {

                // Determine a score relative to the original query

                // Determine the query and make it compatible with our metric
                // class
                // by splitting the single terms
                String[] queryTerms = params.getParams(CommonParams.Q);
                final Collection<String> queryTermsCollection = new ArrayList<>();
                for (String s : queryTerms) {
                    // Split at whitespace except we have a quoted term
                    Matcher matcher = WHITE_SPACES_WITH_QUOTES_SPLITTING_PATTERN.matcher(s);
                    while (matcher.find()) {
                        queryTermsCollection.add(matcher.group().replaceAll("^\"|\"$", ""));
                    }
                }

                // In some contexts, i.e. in KWC that are derived from ordinary
                // keywords or if
                // wildcards occur, also add all the query terms as a single
                // phrase term
                // with stripped wildcards
                StringBuilder sb = new StringBuilder();
                for (String s : queryTermsCollection) {
                    s = s.replace("*", "");
                    sb.append(s);
                    sb.append(" ");
                }

                queryTermsCollection.add(sb.toString().trim());

                final ArrayList<String> queryList = new ArrayList<>(queryTermsCollection);
                final String facetfield = params.get(FacetParams.FACET_FIELD);

                // Get the current facet entry and make it compatible with our
                // metric class
                // "facet_fields" itself contains a NamedList with the
                // facet.field as key

                final NamedList<Object> facetFieldsNamedList = (NamedList<Object>) counts.get("facet_fields");
                final NamedList<Object> facetFields = (NamedList<Object>) facetFieldsNamedList.get(facetfield);

                final List<Entry<Entry<String, Object>, Double>> facetPrefixListScored = new ArrayList<>();
                for (final Entry<String, Object> entry : facetFields) {
                    final String facetTerms = entry.getKey();

                    // Split up each KWC and calculate the scoring

                    ArrayList<String> facetList = new ArrayList<>(Arrays.asList(facetTerms.split("/|\\\\/")));

                    // For usability reasons sort the result facets according to
                    // the order of the search
                    facetList = KeywordSort.sortToReferenceChain(queryList, facetList);

                    final double score = KeywordChainMetric.calculateSimilarityScore(queryList, facetList);

                    // Collect the result in a sorted list and throw away
                    // garbage
                    if (score > 0) {
                        String facetTermsSorted = StringUtils.join(facetList, "/");
                        Map.Entry<String, Object> sortedEntry = new AbstractMap.SimpleEntry<>(facetTermsSorted, entry.getValue());
                        facetPrefixListScored.add(new AbstractMap.SimpleEntry<>(sortedEntry, score));
                    }
                }

                Collections.sort(facetPrefixListScored, ENTRY_COMPARATOR);

                // Extract all the values wrap it back to NamedList again and
                // replace in the original structure

                facetFieldsNamedList.clear();
                NamedList<Object> facetNamedListSorted = new NamedList<>();

                // We had to disable all limits and offsets sort according
                // Handle this accordingly now

                int offset = (params.getInt(FacetParams.FACET_OFFSET) != null) ? params.getInt(FacetParams.FACET_OFFSET) : 0;
                int limit = (params.getInt(FacetParams.FACET_LIMIT) != null) ? params.getInt(FacetParams.FACET_LIMIT) : 100;

                // Strip uneeded elements
                int s = facetPrefixListScored.size();
                int off = (offset < s) ? offset : 0;
                limit = (limit < 0) ? s : limit; // Handle a negative limit
                // param, i.e. unlimited results
                int lim = (offset + limit <= s) ? (offset + limit) : s;

                final List<Entry<Entry<String, Object>, Double>> facetPrefixListScoredTruncated = facetPrefixListScored.subList(off, lim);

                for (Entry<Entry<String, Object>, Double> e : facetPrefixListScoredTruncated) {
                    facetNamedListSorted.add(e.getKey().getKey(), e.getKey().getValue());
                }

                facetFieldsNamedList.add(facetfield, facetNamedListSorted);
                NamedList<Object> countList = new NamedList<>();
                countList.add("count", facetPrefixListScored.size());
                facetFieldsNamedList.add(facetfield + "-count", countList);

                counts.remove("facet_fields");
                counts.add("facet_fields", facetFieldsNamedList);
            }

            rb.rsp.add("facet_counts", counts);
        }
    }

    @Override
    public String getDescription() {
        return "Handle Prefix Sort Faceting";
    }
}
