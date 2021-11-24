package org.apache.solr.request;

import static org.apache.solr.common.params.CommonParams.SORT;

import java.io.IOException;
import java.lang.invoke.MethodHandles;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.function.Predicate;

import org.apache.lucene.util.BytesRef;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.SolrException.ErrorCode;
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
import org.apache.solr.search.facet.FacetRequest;
import org.apache.solr.util.RTimer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import de.uni_tuebingen.ub.ixTheo.common.params.FacetPrefixSortParams;

public class SimplePrefixSortFacets extends SimpleFacets {
    private static final Logger LOG = LoggerFactory.getLogger(MethodHandles.lookup().lookupClass());
    protected java.util.concurrent.Executor facetExecutor;


    public SimplePrefixSortFacets(final SolrQueryRequest req, final DocSet docs, final SolrParams params, final ResponseBuilder rb) {
        super(req, docs, params, rb);
    }


    /**
     * @param existsRequested
     *            facet.exists=true is passed for the given field
     */
    static FacetMethod selectFacetMethod(String fieldName, SchemaField field, FacetMethod method, Integer mincount, boolean existsRequested) {
        if (existsRequested) {
            checkMincountOnExists(fieldName, mincount);
            if (method == null) {
                method = FacetMethod.ENUM;
            }
        }
        final FacetMethod facetMethod = selectFacetMethod(field, method, mincount);

        if (existsRequested && facetMethod != FacetMethod.ENUM) {
            throw new SolrException(ErrorCode.BAD_REQUEST, FacetParams.FACET_EXISTS + "=true is requested, but " + FacetParams.FACET_METHOD + "="
                    + FacetParams.FACET_METHOD_enum + " can't be used with " + fieldName);
        }
        return facetMethod;
    }


    /**
     * This method will force the appropriate facet method even if the user
     * provided a different one as a request parameter
     *
     * N.B. this method could overwrite what you passed as request parameter. Be
     * Extra careful
     *
     * @param field
     *            field we are faceting
     * @param method
     *            the facet method passed as a request parameter
     * @param mincount
     *            the minimum value a facet should have to be returned
     * @return the FacetMethod to use
     */
    static FacetMethod selectFacetMethod(SchemaField field, FacetMethod method, Integer mincount) {

        FieldType type = field.getType();
        if (type.isPointField()) {
            // Only FCS is supported for PointFields for now
            return FacetMethod.FCS;
        }

        /* The user did not specify any preference */
        if (method == null) {
            /*
             * Always use filters for booleans if not DocValues only... we know
             * the number of values is very small.
             */
            if (type instanceof BoolField && (field.indexed() == true || field.hasDocValues() == false)) {
                method = FacetMethod.ENUM;
            } else if (type.getNumberType() != null && !field.multiValued()) {
                /*
                 * the per-segment approach is optimal for numeric field types
                 * since there are no global ords to merge and no need to create
                 * an expensive top-level reader
                 */
                method = FacetMethod.FCS;
            } else {
                // TODO: default to per-segment or not?
                method = FacetMethod.FC;
            }
        }

        /* FC without docValues does not support single valued numeric facets */
        if (method == FacetMethod.FC && type.getNumberType() != null && !field.multiValued()) {
            method = FacetMethod.FCS;
        }

        /*
         * UIF without DocValues can't deal with mincount=0, the reason is
         * because we create the buckets based on the values present in the
         * result set. So we are not going to see facet values which are not in
         * the result set
         */
        if (method == FacetMethod.UIF && !field.hasDocValues() && mincount == 0) {
            method = field.multiValued() ? FacetMethod.FC : FacetMethod.FCS;
        }

        /*
         * ENUM can't deal with trie fields that index several terms per value
         */
        if (method == FacetMethod.ENUM && TrieField.getMainValuePrefix(type) != null) {
            method = field.multiValued() ? FacetMethod.FC : FacetMethod.FCS;
        }

        /* FCS can't deal with multi token fields */
        final boolean multiToken = field.multiValued() || type.multiValuedFieldCache();
        if (method == FacetMethod.FCS && multiToken) {
            method = FacetMethod.FC;
        }

        return method;
    }


    /**
     * Term counts for use in field faceting that resepects the appropriate
     * mincount
     *
     * @see FacetParams#FACET_MINCOUNT
     */
    public NamedList<Integer> getTermCounts(String field, ParsedParams parsed) throws IOException {
        Integer mincount = parsed.params.getFieldInt(field, FacetParams.FACET_MINCOUNT);
        return getTermCounts(field, mincount, parsed);
    }


    /**
     * Term counts for use in field faceting that resepcts the specified
     * mincount - if mincount is null, the "zeros" param is consulted for the
     * appropriate backcompat default
     *
     * @see FacetParams#FACET_ZEROS
     */
    private NamedList<Integer> getTermCounts(String field, Integer mincount, ParsedParams parsed) throws IOException {
        final SolrParams params = parsed.params;
        final DocSet docs = parsed.docs;
        final int threads = parsed.threads;
        int offset = params.getFieldInt(field, FacetParams.FACET_OFFSET, 0);
        int limit = params.getFieldInt(field, FacetParams.FACET_LIMIT, 100);
        if (limit == 0)
            return new NamedList<>();
        if (params.get(FacetParams.FACET_SORT).equals(FacetPrefixSortParams.FACET_SORT_PREFIX)) {
            limit = -1;
            offset = 0;
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

        final Predicate<BytesRef> termFilter = newBytesRefFilter(field, params);

        boolean exists = params.getFieldBool(field, FacetParams.FACET_EXISTS, false);

        NamedList<Integer> counts;
        SchemaField sf = searcher.getSchema().getField(field);
        if (sf.getType().isPointField() && !sf.hasDocValues()) {
            throw new SolrException(SolrException.ErrorCode.BAD_REQUEST, "Can't facet on a PointField without docValues");
        }
        FieldType ft = sf.getType();

        // determine what type of faceting method to use
        final String methodStr = params.getFieldParam(field, FacetParams.FACET_METHOD);
        final FacetMethod requestedMethod;
        if (FacetParams.FACET_METHOD_enum.equals(methodStr)) {
            requestedMethod = FacetMethod.ENUM;
        } else if (FacetParams.FACET_METHOD_fcs.equals(methodStr)) {
            requestedMethod = FacetMethod.FCS;
        } else if (FacetParams.FACET_METHOD_fc.equals(methodStr)) {
            requestedMethod = FacetMethod.FC;
        } else if (FacetParams.FACET_METHOD_uif.equals(methodStr)) {
            requestedMethod = FacetMethod.UIF;
        } else {
            requestedMethod = null;
        }

        final boolean multiToken = sf.multiValued() || ft.multiValuedFieldCache();

        FacetMethod appliedFacetMethod = selectFacetMethod(field, sf, requestedMethod, mincount, exists);

        RTimer timer = null;
        if (fdebug != null) {
            fdebug.putInfoItem("requestedMethod", requestedMethod == null ? "not specified" : requestedMethod.name());
            fdebug.putInfoItem("appliedMethod", appliedFacetMethod.name());
            fdebug.putInfoItem("inputDocSetSize", docs.size());
            fdebug.putInfoItem("field", field);
            timer = new RTimer();
        }

        if (params.getFieldBool(field, GroupParams.GROUP_FACET, false)) {
            counts = getGroupedCounts(searcher, docs, field, multiToken, offset, limit, mincount, missing, sort, prefix, termFilter);
        } else {
            assert appliedFacetMethod != null;
            switch (appliedFacetMethod) {
            case ENUM:
                assert TrieField.getMainValuePrefix(ft) == null;
                counts = getFacetTermEnumCounts(searcher, docs, field, offset, limit, mincount, missing, sort, prefix, termFilter, exists);
                break;
            case FCS:
                assert ft.isPointField() || !multiToken;
                if (ft.isPointField() || (ft.getNumberType() != null && !sf.multiValued())) {
                    if (prefix != null) {
                        throw new SolrException(ErrorCode.BAD_REQUEST, FacetParams.FACET_PREFIX + " is not supported on numeric types");
                    }
                    if (termFilter != null) {
                        throw new SolrException(ErrorCode.BAD_REQUEST, "BytesRef term filters (" + FacetParams.FACET_MATCHES + ", " + FacetParams.FACET_CONTAINS
                                + ", " + FacetParams.FACET_EXCLUDETERMS + ") are not supported on numeric types");
                    }
                    if (ft.isPointField() && mincount <= 0) { // default is
                                                              // mincount=0. See
                                                              // SOLR-10033 &
                                                              // SOLR-11174.
                        String warningMessage = "Raising facet.mincount from " + mincount + " to 1, because field " + field + " is Points-based.";
                        LOG.warn(warningMessage);
                        List<String> warnings = (List<String>) rb.rsp.getResponseHeader().get("warnings");
                        if (null == warnings) {
                            warnings = new ArrayList<>();
                            rb.rsp.getResponseHeader().add("warnings", warnings);
                        }
                        warnings.add(warningMessage);

                        mincount = 1;
                    }
                    counts = NumericFacets.getCounts(searcher, docs, field, offset, limit, mincount, missing, sort);
                } else {
                    PerSegmentSingleValuedFaceting ps = new PerSegmentSingleValuedFaceting(searcher, docs, field, offset, limit, mincount, missing, sort,
                            prefix, termFilter);
                    Executor executor = threads == 0 ? directExecutor : facetExecutor;
                    ps.setNumThreads(threads);
                    counts = ps.getFacetCounts(executor);
                }
                break;
            case UIF:
                // Emulate the JSON Faceting structure so we can use the same
                // parsing classes
                Map<String, Object> jsonFacet = new HashMap<>(13);
                jsonFacet.put("type", "terms");
                jsonFacet.put("field", field);
                jsonFacet.put("offset", offset);
                jsonFacet.put("limit", limit);
                jsonFacet.put("mincount", mincount);
                jsonFacet.put("missing", missing);
                jsonFacet.put("prefix", prefix);
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
                jsonFacet.put(SORT, sortVal );

                //TODO do we handle debug?  Should probably already be handled by the legacy code

                Object resObj = FacetRequest.parseOneFacetReq(req, jsonFacet).process(req, docs);
                //Go through the response to build the expected output for SimpleFacets
                counts = new NamedList<>();
                if(resObj != null) {
                  NamedList<Object> res = (NamedList<Object>) resObj;

                  List<NamedList<Object>> buckets = (List<NamedList<Object>>)res.get("buckets");
                  for(NamedList<Object> b : buckets) {
                    counts.add(b.get("val").toString(), (Integer)b.get("count"));
                  }
                  if(missing) {
                    NamedList<Object> missingCounts = (NamedList<Object>) res.get("missing");
                    counts.add(null, (Integer)missingCounts.get("count"));
                  }
                }
                break;
            case FC:
                counts = DocValuesFacets.getCounts(searcher, docs, field, offset, limit, mincount, missing, sort, prefix, termFilter, fdebug);
                break;
            default:
                throw new AssertionError();
            }
        }

        if (fdebug != null) {
            long timeElapsed = (long) timer.getTime();
            fdebug.setElapse(timeElapsed);
        }

        return counts;
    }


    enum FacetMethod {
        ENUM, FC, FCS, UIF;
    }
}
