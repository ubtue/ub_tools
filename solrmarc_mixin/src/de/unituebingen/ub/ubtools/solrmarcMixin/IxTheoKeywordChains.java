package de.unituebingen.ub.ubtools.solrmarcMixin;


import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.Subfield;
import org.marc4j.marc.VariableField;
import org.solrmarc.index.SolrIndexerMixin;

import java.util.*;


public class IxTheoKeywordChains extends SolrIndexerMixin {

    private final static String KEYWORD_DELIMITER = "/";
    private final static String SUBFIELD_CODES = "at";

    public Set<String> getKeyWordChain(final Record record, final String fieldSpec) {
        final List<VariableField> variableFields = record.getVariableFields(fieldSpec);
        final Map<Character, List<String>> keyWordChains = new HashMap<>();

        for (final VariableField variableField : variableFields) {
            final DataField dataField = (DataField) variableField;
            processField(dataField, keyWordChains);
        }
        return concatenateKeyWordsToChains(keyWordChains);
    }

    public Set<String> getKeyWordChainBag(final Record record, final String fieldSpec) {
        final List<VariableField> variableFields = record.getVariableFields(fieldSpec);
        final Set<String> keyWordChainsBag = new HashSet<>();

        for (final VariableField variableField : variableFields) {
            final DataField dataField = (DataField) variableField;
            final List<Subfield> subfields = dataField.getSubfields('a');
            for (final Subfield subfield : subfields) {
                final String subfield_data = subfield.getData();
                if (subfield_data.length() > 1) {
                    keyWordChainsBag.add(subfield_data);
                }
            }
        }
        return keyWordChainsBag;
    }

    public Set<String> getKeyWordChainSorted(final Record record, final String fieldSpec) {
        final List<VariableField> variableFields = record.getVariableFields(fieldSpec);
        final Map<Character, List<String>> keyWordChains = new HashMap<>();

        for (final VariableField variableField : variableFields) {
            final DataField dataField = (DataField) variableField;
            processField(dataField, keyWordChains);

            // Sort keyword chain
            final char chainID = dataField.getIndicator1();
            final List<String> keyWordChain = getKeyWordChain(keyWordChains, chainID);
            Collections.sort(keyWordChain);
        }
        return concatenateKeyWordsToChains(keyWordChains);
    }

    /**
     * Extracts the keyword from data field and inserts it into the right keyword chain.
     */
    private void processField(final DataField dataField, final Map<Character, List<String>> keyWordChains) {
        final char chainID = dataField.getIndicator1();
        final List<String> keyWordChain = getKeyWordChain(keyWordChains, chainID);

        for (int i = 0; i < SUBFIELD_CODES.length(); ++i) {
            final char subfieldCode = SUBFIELD_CODES.charAt(i);
            final List<Subfield> subfields = dataField.getSubfields(subfieldCode);

            for (final Subfield subfield : subfields) {
                if (subfield.getData().length() > 1) {
                    final String keyWord = subfield.getData();
                    keyWordChain.add(keyWord);
                    return;
                }
            }
        }
    }

    /**
     * Finds the right keyword chain for a given chain id.
     *
     * @return A map containing the keywords of the chain (id -> keyword), or an empty map.
     */
    private List<String> getKeyWordChain(final Map<Character, List<String>> keyWordChains, final char chainID) {
        List<String> keyWordChain = keyWordChains.get(chainID);
        if (keyWordChain == null) {
            keyWordChain = new ArrayList<>();
            keyWordChains.put(chainID, keyWordChain);
        }

        return keyWordChain;
    }

    private Set<String> concatenateKeyWordsToChains(final Map<Character, List<String>> keyWordChains) {
        final List<Character> chainIDs = new ArrayList<>(keyWordChains.keySet());
        Collections.sort(chainIDs);

        final Set<String> chainSet = new LinkedHashSet<>();
        for (final Character chainID : chainIDs) {
            chainSet.add(keyChainToString(keyWordChains.get(chainID)));
        }
        return chainSet;
    }

    private String keyChainToString(final List<String> keyWordChain) {
        final StringBuilder buffer = new StringBuilder();
        for (final String keyWord : keyWordChain) {
            buffer.append(KEYWORD_DELIMITER);
            buffer.append(keyWord);
        }

        if (buffer.length() == 0) {
            return "";
        }
        // Discard leading keyword delimiter.
        return buffer.toString().substring(1);
    }
}
