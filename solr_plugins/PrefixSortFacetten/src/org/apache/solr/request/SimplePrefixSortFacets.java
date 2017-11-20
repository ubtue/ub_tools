package org.apache.solr.request;


import de.uni_tuebingen.ub.ixTheo.common.params.FacetPrefixSortParams;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.params.FacetParams;
import org.apache.solr.common.params.GroupParams;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.SimpleOrderedMap;
import org.apache.solr.handler.component.ResponseBuilder;
import org.apache.solr.schema.BoolField;
import org.apache.solr.schema.FieldType;
import org.apache.solr.schema.SchemaField;
import org.apache.solr.schema.TrieField;
import org.apache.solr.search.DocSet;
import org.apache.solr.search.facet.FacetDebugInfo;
import org.apache.solr.search.facet.FacetProcessor;
import org.apache.solr.request.SimpleFacets;

import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;


public class SimplePrefixSortFacets extends SimpleFacets {

    public SimplePrefixSortFacets(final SolrQueryRequest req, final DocSet docs, final SolrParams params, final ResponseBuilder rb) {
        super(req, docs, params, rb);
    }

    /**
     * Term counts for use in field faceting that resepects the appropriate mincount
     *
     * @see FacetParams#FACET_MINCOUNT
     */
    public NamedList<Integer> getTermCounts(String field, ParsedParams parsed) throws IOException {
        Integer mincount = parsed.params.getFieldInt(field, FacetParams.FACET_MINCOUNT);
        return getTermCounts(field, mincount, parsed);
    }

    /**
     * Term counts for use in field faceting that resepcts the specified mincount -
     * if mincount is null, the "zeros" param is consulted for the appropriate backcompat
     * default
     *
     * @see FacetParams#FACET_ZEROS
     */
    private NamedList<Integer> getTermCounts(String field, Integer mincount, ParsedParams parsed) throws IOException {
        final SolrParams params = parsed.params;
        final DocSet docs = parsed.docs;
        final int threads = parsed.threads;
        int offset = params.getFieldInt(field, FacetParams.FACET_OFFSET, 0);
        int limit = params.getFieldInt(field, FacetParams.FACET_LIMIT, 100);
        if (limit == 0) return new NamedList<>();
        if (params.get(FacetParams.FACET_SORT).equals(FacetPrefixSortParams.FACET_SORT_PREFIX)) {
            limit = -1;
        }
        if (mincount == null) {
            Boolean zeros = params.getFieldBool(field, FacetParams.FACET_ZEROS);
            // mincount = (zeros!=null && zeros) ? 0 : 1;
            mincount = (zeros != null && !zeros) ? 1 : 0;
            // current default is to include zeros.
        }
        boolean missing = params.getFieldBool(field, FacetParams.FACET_MISSING, false);
        // default to sorting if there is a limit.
        String sort = params.getFieldParam(field, FacetParams.FACET_SORT, limit > 0 ? FacetParams.FACET_SORT_COUNT : FacetParams.FACET_SORT_INDEX);
        String prefix = params.getFieldParam(field, FacetParams.FACET_PREFIX);
        String contains = params.getFieldParam(field, FacetParams.FACET_CONTAINS);
        boolean ignoreCase = params.getFieldBool(field, FacetParams.FACET_CONTAINS_IGNORE_CASE, false);

        NamedList<Integer> counts;
        SchemaField sf = searcher.getSchema().getField(field);
        FieldType ft = sf.getType();

        // determine what type of faceting method to use
        final String methodStr = params.getFieldParam(field, FacetParams.FACET_METHOD);
        FacetMethod method = null;
        if (FacetParams.FACET_METHOD_enum.equals(methodStr)) {
            method = FacetMethod.ENUM;
        } else if (FacetParams.FACET_METHOD_fcs.equals(methodStr)) {
            method = FacetMethod.FCS;
        } else if (FacetParams.FACET_METHOD_fc.equals(methodStr)) {
            method = FacetMethod.FC;
        } else if (FacetParams.FACET_METHOD_uif.equals(methodStr)) {
            method = FacetMethod.UIF;
        }

        if (method == FacetMethod.ENUM && TrieField.getMainValuePrefix(ft) != null) {
            // enum can't deal with trie fields that index several terms per value
            method = sf.multiValued() ? FacetMethod.FC : FacetMethod.FCS;
        }

        if (method == null && ft instanceof BoolField) {
            // Always use filters for booleans... we know the number of values is very small.
            method = FacetMethod.ENUM;
        }

        final boolean multiToken = sf.multiValued() || ft.multiValuedFieldCache();

        if (ft.getNumericType() != null && !sf.multiValued()) {
            // the per-segment approach is optimal for numeric field types since there
            // are no global ords to merge and no need to create an expensive
            // top-level reader
            method = FacetMethod.FCS;
        }

        if (method == null) {
            // TODO: default to per-segment or not?
            method = FacetMethod.FC;
        }

        if (method == FacetMethod.FCS && multiToken) {
            // only fc knows how to deal with multi-token fields
            method = FacetMethod.FC;
        }

        if (method == FacetMethod.ENUM && sf.hasDocValues()) {
            // only fc can handle docvalues types
            method = FacetMethod.FC;
        }

        if (params.getFieldBool(field, GroupParams.GROUP_FACET, false)) {
            counts = getGroupedCounts(searcher, docs, field, multiToken, offset, limit, mincount, missing, sort, prefix, contains, ignoreCase);
        } else {
            assert method != null;
            switch (method) {
                case ENUM:
                    assert TrieField.getMainValuePrefix(ft) == null;
                    counts = getFacetTermEnumCounts(searcher, docs, field, offset, limit, mincount, missing, sort, prefix, contains, ignoreCase, false);
                    break;
                case FCS:
                    assert !multiToken;
                    if (ft.getNumericType() != null && !sf.multiValued()) {
                        // force numeric faceting
                        if (prefix != null && !prefix.isEmpty()) {
                            throw new SolrException(SolrException.ErrorCode.BAD_REQUEST, FacetParams.FACET_PREFIX + " is not supported on numeric types");
                        }
                        if (contains != null && !contains.isEmpty()) {
                            throw new SolrException(SolrException.ErrorCode.BAD_REQUEST, FacetParams.FACET_CONTAINS + " is not supported on numeric types");
                        }
                        counts = NumericFacets.getCounts(searcher, docs, field, offset, limit, mincount, missing, sort);
                    } else {
                        PerSegmentSingleValuedFaceting ps = new PerSegmentSingleValuedFaceting(searcher, docs, field, offset, limit, mincount, missing, sort, prefix, contains, ignoreCase);
                        Executor executor = threads == 0 ? directExecutor : facetExecutor;
                        ps.setNumThreads(threads);
                        counts = ps.getFacetCounts(executor);
                    }
                    break;
                case UIF:

                    //Emulate the JSON Faceting structure so we can use the same parsing classes
                    Map<String, Object> jsonFacet = new HashMap<>(13);
                    jsonFacet.put("type", "terms");
                    jsonFacet.put("field", field);
                    jsonFacet.put("offset", offset);
                    jsonFacet.put("limit", limit);
                    jsonFacet.put("mincount", mincount);
                    jsonFacet.put("missing", missing);

                    if (prefix != null) {
                        // presumably it supports single-value, but at least now returns wrong results on multi-value
                        throw new SolrException(
                                SolrException.ErrorCode.BAD_REQUEST,
                                FacetParams.FACET_PREFIX + "=" + prefix +
                                        " are not supported by " + FacetParams.FACET_METHOD + "=" + FacetParams.FACET_METHOD_uif +
                                        " for field:" + field
                                //jsonFacet.put("prefix", prefix);
                        );
                    }
                    jsonFacet.put("numBuckets", params.getFieldBool(field, "numBuckets", false));
                    jsonFacet.put("allBuckets", params.getFieldBool(field, "allBuckets", false));
                    jsonFacet.put("method", "uif");
                    jsonFacet.put("cacheDf", 0);
                    jsonFacet.put("perSeg", false);

                    final String sortVal;
                    switch (sort) {
                        case FacetParams.FACET_SORT_COUNT_LEGACY:
                            sortVal = FacetParams.FACET_SORT_COUNT;
                            break;
                        case FacetParams.FACET_SORT_INDEX_LEGACY:
                            sortVal = FacetParams.FACET_SORT_INDEX;
                            break;
                        default:
                            sortVal = sort;
                    }
                    jsonFacet.put("sort", sortVal);

                    Map<String, Object> topLevel = new HashMap<>();
                    topLevel.put(field, jsonFacet);

                    topLevel.put("processEmpty", true);

                    FacetProcessor fproc = FacetProcessor.createProcessor(rb.req, topLevel, // rb.getResults().docSet
                            docs);
                    //TODO do we handle debug?  Should probably already be handled by the legacy code
                    fproc.process();

                    //Go through the response to build the expected output for SimpleFacets
                    Object res = fproc.getResponse();
                    counts = new NamedList<Integer>();
                    if (res != null) {
                        SimpleOrderedMap<Object> som = (SimpleOrderedMap<Object>) res;
                        SimpleOrderedMap<Object> asdf = (SimpleOrderedMap<Object>) som.get(field);

                        List<SimpleOrderedMap<Object>> buckets = (List<SimpleOrderedMap<Object>>) asdf.get("buckets");
                        for (SimpleOrderedMap<Object> b : buckets) {
                            counts.add(b.get("val").toString(), (Integer) b.get("count"));
                        }
                        if (missing) {
                            SimpleOrderedMap<Object> missingCounts = (SimpleOrderedMap<Object>) asdf.get("missing");
                            counts.add(null, (Integer) missingCounts.get("count"));
                        }
                    }
                    break;
                case FC:
                    FacetDebugInfo info = new FacetDebugInfo();
                    counts = DocValuesFacets.getCounts(searcher, docs, field, offset, limit, mincount, missing, sort, prefix, contains, ignoreCase, info);
                    break;
                default:
                    throw new AssertionError();
            }
        }

        return counts;
    }

    enum FacetMethod {
        ENUM, FC, FCS, UIF;
    }
}
