package de.unituebingen.ub.elasticsearch.plugin;
import java.util.Collections;
import java.util.Map;

import org.elasticsearch.plugins.Plugin;
import org.elasticsearch.plugins.SearchPlugin;
import org.elasticsearch.search.fetch.subphase.highlight.Highlighter;
import de.unituebingen.ub.elasticsearch.TueFindHighlighter;


public class TueFindHighlighterPlugin extends Plugin implements SearchPlugin {
    @Override
    public Map<String, Highlighter> getHighlighters() {
         return  Collections.singletonMap(TueFindHighlighter.NAME, new TueFindHighlighter());
    }
}

