/*
 * Licensed to Elasticsearch under one or more contributor
 * license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright
 * ownership. Elasticsearch licenses this file to you under
 * the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

// Heavily derived from CustomUnifiedHighlighter

package de.unituebingen.ub.elasticsearch;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.index.Term;
import org.apache.lucene.queries.CommonTermsQuery;
import org.apache.lucene.search.DocIdSetIterator;
import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.PrefixQuery;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.TermQuery;
import org.apache.lucene.search.spans.SpanMultiTermQueryWrapper;
import org.apache.lucene.search.spans.SpanNearQuery;
import org.apache.lucene.search.spans.SpanOrQuery;
import org.apache.lucene.search.spans.SpanQuery;
import org.apache.lucene.search.spans.SpanTermQuery;
import org.apache.lucene.search.uhighlight.UHComponents;
import org.apache.lucene.util.BytesRef;
import org.apache.lucene.util.automaton.CharacterRunAutomaton;
import org.elasticsearch.common.Nullable;
import org.elasticsearch.common.logging.DeprecationLogger;
import org.elasticsearch.common.logging.Loggers;
import org.elasticsearch.common.lucene.all.AllTermQuery;
import org.elasticsearch.common.lucene.search.MultiPhrasePrefixQuery;
import org.elasticsearch.common.lucene.search.function.FunctionScoreQuery;
import org.elasticsearch.index.IndexSettings;
import org.elasticsearch.index.search.ESToParentBlockJoinQuery;

import java.io.IOException;
import java.text.BreakIterator;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import org.apache.lucene.search.uhighlight.Snippet;
import org.apache.lucene.search.uhighlight.BoundedBreakIteratorScanner;
import org.apache.lucene.search.uhighlight.CustomPassageFormatter;
import org.apache.lucene.search.uhighlight.CustomSeparatorBreakIterator;
import org.apache.lucene.search.uhighlight.CustomUnifiedHighlighter;
//import org.apache.lucene.search.uhighlight.UnifiedHighlighter.OffsetSource;
import org.apache.lucene.search.uhighlight.UnifiedHighlighter;
//import org.elasticsearch.search.fetch.subphase.highlight.UnifiedHighlighter;
import org.apache.lucene.search.uhighlight.PassageFormatter;
import org.apache.lucene.search.uhighlight.FieldHighlighter;
import org.apache.lucene.search.uhighlight.PhraseHelper;
import org.apache.lucene.search.uhighlight.SplittingBreakIterator;
//import org.apache.lucene.search.uhighlight.CustomFieldHighlighter;
import org.apache.lucene.search.uhighlight.FieldOffsetStrategy;
/**
 * Subclass of the {@link UnifiedHighlighter} that works for a single field in a single document.
 * Uses a custom {@link PassageFormatter}. Accepts field content as a constructor
 * argument, given that loadings field value can be done reading from _source field.
 * Supports using different {@link BreakIterator} to break the text into fragments. Considers every distinct field
 * value as a discrete passage for highlighting (unless the whole content needs to be highlighted).
 * Supports both returning empty snippets and non highlighted snippets when no highlighting can be performed.
 */
public class TueFindCustomHighlighter extends CustomUnifiedHighlighter {

    private final OffsetSource offsetSource;
    private final String fieldValue;
    private final PassageFormatter passageFormatter;
    private final BreakIterator breakIterator;
    private final Locale breakIteratorLocale;
    private final int noMatchSize;


    /**
     * Creates a new instance of {@link TueFindCustomHighlighter}
     *
     * @param analyzer the analyzer used for the field at index time, used for multi term queries internally.
     * @param passageFormatter our own {@link CustomPassageFormatter}
     *                    which generates snippets in forms of {@link Snippet} objects.
     * @param offsetSource the {@link OffsetSource} to used for offsets retrieval.
     * @param breakIteratorLocale the {@link Locale} to use for dividing text into passages.
     *                    If null {@link Locale#ROOT} is used.
     * @param breakIterator the {@link BreakIterator} to use for dividing text into passages.
     *                    If null {@link BreakIterator#getSentenceInstance(Locale)} is used.
     * @param fieldValue the original field values delimited by MULTIVAL_SEP_CHAR.
     * @param noMatchSize The size of the text that should be returned when no highlighting can be performed.
     * @param maxAnalyzedOffset The maximum number of characters that will be analyzed for highlighting.
     */
    public TueFindCustomHighlighter(IndexSearcher searcher,
                                    Analyzer analyzer,
                                    OffsetSource offsetSource,
                                    PassageFormatter passageFormatter,
                                    @Nullable Locale breakIteratorLocale,
                                    @Nullable BreakIterator breakIterator,
                                    String fieldValue,
                                    int noMatchSize) {
        super(searcher, analyzer, offsetSource, passageFormatter, breakIteratorLocale, 
        breakIterator, fieldValue, noMatchSize);
        this.offsetSource = offsetSource;
        this.breakIterator = breakIterator;
        this.breakIteratorLocale = breakIteratorLocale == null ? Locale.ROOT : breakIteratorLocale;
        this.passageFormatter = passageFormatter;
        this.fieldValue = fieldValue;
        this.noMatchSize = noMatchSize;

    }

    @Override
    protected FieldHighlighter getFieldHighlighter(String field, Query query, Set<Term> allTerms, int maxPassages) {
        BytesRef[] terms = filterExtractedTerms(getFieldMatcher(field), allTerms);
        Set<HighlightFlag> highlightFlags = getFlags(field);
        PhraseHelper phraseHelper = getPhraseHelper(field, query, highlightFlags);
        CharacterRunAutomaton[] automata = getAutomata(field, query, highlightFlags);
        OffsetSource offsetSource = getOptimizedOffsetSource(field, terms, phraseHelper, automata);
        BreakIterator breakIterator = new TueFindSplittingBreakIterator(getBreakIterator(field),
            UnifiedHighlighter.MULTIVAL_SEP_CHAR);
        UHComponents components = new UHComponents(field, getFieldMatcher(field), query, terms, phraseHelper, automata, highlightFlags);
        FieldOffsetStrategy strategy =
            getOffsetStrategy(offsetSource, components);
        return new TueFindCustomFieldHighlighter(field, strategy, breakIteratorLocale, breakIterator,
            getScorer(field), maxPassages, (noMatchSize > 0 ? 1 : 0), getFormatter(field), noMatchSize, fieldValue);
    }


}
