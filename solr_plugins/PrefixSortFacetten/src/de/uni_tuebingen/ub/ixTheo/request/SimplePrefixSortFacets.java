package de.uni_tuebingen.ub.ixTheo.request;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.EnumSet;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.RunnableFuture;
import java.util.concurrent.Semaphore;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import org.apache.lucene.index.AtomicReader;
import org.apache.lucene.index.DocsEnum;
import org.apache.lucene.index.Fields;
import org.apache.lucene.index.MultiDocsEnum;
import org.apache.lucene.index.SortedDocValues;
import org.apache.lucene.index.Term;
import org.apache.lucene.index.Terms;
import org.apache.lucene.index.TermsEnum;
import org.apache.lucene.search.DocIdSetIterator;
import org.apache.lucene.search.FieldCache;
import org.apache.lucene.search.Filter;
import org.apache.lucene.search.MatchAllDocsQuery;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.TermQuery;
import org.apache.lucene.search.TermRangeQuery;
import org.apache.lucene.search.grouping.AbstractAllGroupHeadsCollector;
import org.apache.lucene.search.grouping.term.TermAllGroupsCollector;
import org.apache.lucene.search.grouping.term.TermGroupFacetCollector;
import org.apache.lucene.util.BytesRef;
import org.apache.lucene.util.BytesRefBuilder;
import org.apache.lucene.util.CharsRef;
import org.apache.lucene.util.CharsRefBuilder;
import org.apache.lucene.util.StringHelper;
import org.apache.lucene.util.UnicodeUtil;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.SolrException.ErrorCode;
import org.apache.solr.common.params.CommonParams;
import org.apache.solr.common.params.FacetParams;
import org.apache.solr.common.params.FacetParams.FacetRangeInclude;
import org.apache.solr.common.params.FacetParams.FacetRangeOther;
import org.apache.solr.common.params.GroupParams;
import org.apache.solr.common.params.RequiredSolrParams;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.common.util.SimpleOrderedMap;
import org.apache.solr.common.util.StrUtils;
import org.apache.solr.handler.component.ResponseBuilder;
//import org.apache.solr.request.IntervalFacets.FacetInterval;
import org.apache.solr.schema.BoolField;
import org.apache.solr.schema.DateField;
import org.apache.solr.schema.FieldType;
import org.apache.solr.schema.IndexSchema;
import org.apache.solr.schema.SchemaField;
import org.apache.solr.schema.SortableDoubleField;
import org.apache.solr.schema.SortableFloatField;
import org.apache.solr.schema.SortableIntField;
import org.apache.solr.schema.SortableLongField;
import org.apache.solr.schema.TrieField;
import org.apache.solr.search.BitDocSet;
import org.apache.solr.search.DocIterator;
import org.apache.solr.search.DocSet;
import org.apache.solr.search.Grouping;
import org.apache.solr.search.HashDocSet;
import org.apache.solr.search.QParser;
import org.apache.solr.search.QueryParsing;
import org.apache.solr.search.SolrIndexSearcher;
import org.apache.solr.search.SortedIntDocSet;
import org.apache.solr.search.SyntaxError;
import org.apache.solr.search.grouping.GroupingSpecification;
import org.apache.solr.util.BoundedTreeSet;
import org.apache.solr.util.DateMathParser;
import org.apache.solr.util.DefaultSolrThreadFactory;
import org.apache.solr.util.LongPriorityQueue;

import org.apache.solr.handler.component.*;
import org.apache.solr.request.*;
import de.uni_tuebingen.ub.ixTheo.common.params.*;
import de.uni_tuebingen.ub.ixTheo.common.util.*;

public class SimplePrefixSortFacets extends SimpleFacets {

    public SimplePrefixSortFacets(SolrQueryRequest req, DocSet docs, SolrParams params) {
        super(req, docs, params, null);
    }

    public SimplePrefixSortFacets(SolrQueryRequest req, DocSet docs, SolrParams params, ResponseBuilder rb) {

        super(req, docs, params, rb);

    }

    enum FacetMethod {
        ENUM, FC, FCS;
    }

    /**
     * Term counts for use in field faceting that resepcts the specified
     * mincount - if mincount is null, the "zeros" param is consulted for the
     * appropriate backcompat default
     *
     * This should be identical to the original SimpleFacets, however we must
     * pay attention to appropriately handle the offset parameter
     *
     * @see FacetParams#FACET_ZEROS
     */
    private NamedList<Integer> getTermCounts(String field, Integer mincount, DocSet base) throws IOException {

        // If we have a prefix sort, we may not limit our results at this stage

        int offset;
        int limit;

        if (params.get(FacetParams.FACET_SORT).equals(FacetPrefixSortParams.FACET_SORT_PREFIX)) {
            offset = 0;
            limit = -1;
        } else {

            offset = params.getFieldInt(field, FacetParams.FACET_OFFSET, 0);
            limit = params.getFieldInt(field, FacetParams.FACET_LIMIT, 100);

            if (limit == 0)
                return new NamedList<>();
        }

        if (mincount == null) {
            Boolean zeros = params.getFieldBool(field, FacetParams.FACET_ZEROS);
            // mincount = (zeros!=null && zeros) ? 0 : 1;
            mincount = (zeros != null && !zeros) ? 1 : 0;
            // current default is to include zeros.
        }
        boolean missing = params.getFieldBool(field, FacetParams.FACET_MISSING, false);
        // default to sorting if there is a limit.
        String sort = params.getFieldParam(field, FacetParams.FACET_SORT,
                limit > 0 ? FacetParams.FACET_SORT_COUNT : FacetParams.FACET_SORT_INDEX);
        String prefix = params.getFieldParam(field, FacetParams.FACET_PREFIX);

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
        }

        if (method == FacetMethod.ENUM && TrieField.getMainValuePrefix(ft) != null) {
            // enum can't deal with trie fields that index several terms per
            // value
            method = sf.multiValued() ? FacetMethod.FC : FacetMethod.FCS;
        }

        if (method == null && ft instanceof BoolField) {
            // Always use filters for booleans... we know the number of values
            // is very small.
            method = FacetMethod.ENUM;
        }

        final boolean multiToken = sf.multiValued() || ft.multiValuedFieldCache();

        if (method == null && ft.getNumericType() != null && !sf.multiValued()) {
            // the per-segment approach is optimal for numeric field types since
            // there
            // are no global ords to merge and no need to create an expensive
            // top-level reader
            method = FacetMethod.FCS;
        }

        if (ft.getNumericType() != null && sf.hasDocValues()) {
            // only fcs is able to leverage the numeric field caches
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
            counts = getGroupedCounts(searcher, base, field, multiToken, offset, limit, mincount, missing, sort,
                    prefix);
        } else {
            assert method != null;
            switch (method) {
            case ENUM:
                assert TrieField.getMainValuePrefix(ft) == null;
                counts = getFacetTermEnumCounts(searcher, base, field, offset, limit, mincount, missing, sort, prefix);
                break;
            case FCS:
                assert !multiToken;
                if (ft.getNumericType() != null && !sf.multiValued()) {
                    // force numeric faceting
                    if (prefix != null && !prefix.isEmpty()) {
                        throw new SolrException(ErrorCode.BAD_REQUEST,
                                FacetParams.FACET_PREFIX + " is not supported on numeric types");
                    }
                    counts = NumericFacets.getCounts(searcher, base, field, offset, limit, mincount, missing, sort);
                } else {
                    PerSegmentSingleValuedFaceting ps = new PerSegmentSingleValuedFaceting(searcher, base, field,
                            offset, limit, mincount, missing, sort, prefix);
                    Executor executor = threads == 0 ? directExecutor : facetExecutor;
                    ps.setNumThreads(threads);
                    counts = ps.getFacetCounts(executor);
                }
                break;
            case FC:
                if (sf.hasDocValues()) {
                    counts = DocValuesFacets.getCounts(searcher, base, field, offset, limit, mincount, missing, sort,
                            prefix);
                } else if (multiToken || TrieField.getMainValuePrefix(ft) != null) {
                    UnInvertedField uif = UnInvertedField.getUnInvertedField(field, searcher);
                    counts = uif.getCounts(searcher, base, offset, limit, mincount, missing, sort, prefix);
                } else {
                    counts = getFieldCacheCounts(searcher, base, field, offset, limit, mincount, missing, sort, prefix);
                }
                break;
            default:
                throw new AssertionError();
            }
        }

        return counts;
    }

    static final Executor directExecutor = new Executor() {
        @Override
        public void execute(Runnable r) {
            r.run();
        }
    };

    /**
     * Looks at various Params to determining if any simple Facet Constraint
     * count computations are desired.
     *
     * @see #getFacetQueryCounts
     * @see #getFacetFieldCounts
     * @see #getFacetDateCounts
     * @see #getFacetRangeCounts
     * @see #getFacetIntervalCounts
     * @see FacetParams#FACET
     * @return a NamedList of Facet Count info or null
     */
    public NamedList<Object> getFacetCounts() {

        // if someone called this method, benefit of the doubt: assume true
        if (!params.getBool(FacetParams.FACET, true))
            return null;

        facetResponse = new SimpleOrderedMap<>();
        try {
            facetResponse.add("facet_queries", getFacetQueryCounts());
            facetResponse.add("facet_fields", getFacetFieldCounts());
            facetResponse.add("facet_dates", getFacetDateCounts());
            facetResponse.add("facet_ranges", getFacetRangeCounts());
            facetResponse.add("facet_intervals", getFacetIntervalCounts());

        } catch (IOException e) {
            throw new SolrException(ErrorCode.SERVER_ERROR, e);
        } catch (SyntaxError e) {
            throw new SolrException(ErrorCode.BAD_REQUEST, e);
        }
        return facetResponse;
    }

    /**
     * Returns a list of value constraints and the associated facet counts for
     * each facet field specified in the params.
     *
     * @see FacetParams#FACET_FIELD
     * @see #getFieldMissingCount
     * @see #getFacetTermEnumCounts
     */
    @SuppressWarnings("unchecked")
    public NamedList<Object> getFacetFieldCounts() throws IOException, SyntaxError {

        NamedList<Object> res = new SimpleOrderedMap<>();
        String[] facetFs = params.getParams(FacetParams.FACET_FIELD);
        if (null == facetFs) {
            return res;
        }

        // Passing a negative number for FACET_THREADS implies an unlimited
        // number of threads is acceptable.
        // Also, a subtlety of directExecutor is that no matter how many times
        // you "submit" a job, it's really
        // just a method call in that it's run by the calling thread.
        int maxThreads = req.getParams().getInt(FacetParams.FACET_THREADS, 0);
        Executor executor = maxThreads == 0 ? directExecutor : facetExecutor;
        final Semaphore semaphore = new Semaphore((maxThreads <= 0) ? Integer.MAX_VALUE : maxThreads);
        List<Future<NamedList>> futures = new ArrayList<>(facetFs.length);

        try {
            // Loop over fields; submit to executor, keeping the future
            for (String f : facetFs) {
                parseParams(FacetParams.FACET_FIELD, f);
                final String termList = localParams == null ? null : localParams.get(CommonParams.TERMS);
                final String workerKey = key;
                final String workerFacetValue = facetValue;
                final DocSet workerBase = this.docs;
                Callable<NamedList> callable = new Callable<NamedList>() {
                    @Override
                    public NamedList call() throws Exception {
                        try {
                            NamedList<Object> result = new SimpleOrderedMap<>();
                            if (termList != null) {
                                List<String> terms = StrUtils.splitSmart(termList, ",", true);
                                result.add(workerKey, getListedTermCounts(workerFacetValue, workerBase, terms));
                            } else {
                                result.add(workerKey, getTermCounts(workerFacetValue, workerBase));
                            }
                            return result;
                        } catch (SolrException se) {
                            throw se;
                        } catch (Exception e) {
                            throw new SolrException(ErrorCode.SERVER_ERROR,
                                    "Exception during facet.field: " + workerFacetValue, e);
                        } finally {
                            semaphore.release();
                        }
                    }
                };

                RunnableFuture<NamedList> runnableFuture = new FutureTask<>(callable);
                semaphore.acquire();// may block and/or interrupt
                executor.execute(runnableFuture);// releases semaphore when done
                futures.add(runnableFuture);
            } // facetFs loop

            // Loop over futures to get the values. The order is the same as
            // facetFs but shouldn't matter.
            for (Future<NamedList> future : futures) {
                res.addAll(future.get());
            }
            assert semaphore.availablePermits() >= maxThreads;
        } catch (InterruptedException e) {
            throw new SolrException(SolrException.ErrorCode.SERVER_ERROR,
                    "Error while processing facet fields: InterruptedException", e);
        } catch (ExecutionException ee) {
            Throwable e = ee.getCause();// unwrap
            if (e instanceof RuntimeException) {
                throw (RuntimeException) e;
            }
            throw new SolrException(SolrException.ErrorCode.SERVER_ERROR,
                    "Error while processing facet fields: " + e.toString(), e);
        }

        return res;
    }

    /**
     * Term counts for use in field faceting that resepects the appropriate
     * mincount
     *
     * @see FacetParams#FACET_MINCOUNT
     */
    public NamedList<Integer> getTermCounts(String field) throws IOException {
        return getTermCounts(field, this.docs);
    }

    /**
     * Term counts for use in field faceting that resepects the appropriate
     * mincount
     *
     * @see FacetParams#FACET_MINCOUNT
     */
    public NamedList<Integer> getTermCounts(String field, DocSet base) throws IOException {
        Integer mincount = params.getFieldInt(field, FacetParams.FACET_MINCOUNT);
        return getTermCounts(field, mincount, base);
    }

    static final Executor facetExecutor = new ThreadPoolExecutor(0, Integer.MAX_VALUE, 10, TimeUnit.SECONDS, // terminate
                                                                                                             // idle
                                                                                                             // threads
                                                                                                             // after
                                                                                                             // 10
                                                                                                             // sec
            new SynchronousQueue<Runnable>() // directly hand off tasks
            , new DefaultSolrThreadFactory("facetExecutor"));

};
