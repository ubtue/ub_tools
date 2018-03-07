package de.unituebingen.ub.elasticsearch;


import org.apache.logging.log4j.Logger;
import org.elasticsearch.common.logging.ESLoggerFactory;
import org.elasticsearch.index.mapper.FieldMapper;
import org.elasticsearch.search.fetch.subphase.highlight.HighlightField;
import org.elasticsearch.search.fetch.subphase.highlight.Highlighter;
import org.elasticsearch.search.fetch.subphase.highlight.HighlighterContext;
import org.elasticsearch.common.text.Text;



public class TueFindHighlighter implements Highlighter {
    public static final String NAME = "tuefind";
    private static final Logger log = ESLoggerFactory.getLogger(TueFindHighlighter.class);


    @Override
    public boolean canHighlight(FieldMapper field) {
        return true;
    }

    @Override
    public HighlightField highlight(HighlighterContext context) {
        log.error("ENTERING highlight()"); 
        final String[] readMe = {"DADADA"};
        log.error("TRYING TO WRITE " + readMe);
        return new HighlightField(context.fieldName, Text.convertFromStringArray(readMe));
    }
}
