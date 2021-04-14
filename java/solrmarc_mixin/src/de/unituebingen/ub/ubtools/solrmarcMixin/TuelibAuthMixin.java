package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.logging.Logger;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.solrmarc.index.SolrIndexerMixin;

public class TuelibAuthMixin extends SolrIndexerMixin {

	protected final static Logger logger = Logger.getLogger(TuelibAuthMixin.class.getName());

	/**
	 * ixTheoKim (GitHub): Wenn ein und dieselbe Standardnummer in mehreren
	 * Varianten vorkommt, von denen eine die lt. Erfassungsformat die korrekte
	 * Version ist, könnte diese verwendet werden. Bsp.: 024 isni:
	 * 0000-0000-2368-8144 024 isni: 0000000023688144 024 isni: 0000 0000 2368 8144
	 * [korrekte Erfassungsform] 024 orcid: 0000-0003-1684-6994 [korrekte
	 * Erfassungsform] 024 orcid: 0000000316846994 024 orcid: 0000 0003 1684 6994
	 * 
	 * Anforderung mtrojan: auch bei einzelnen ISNIs und ORCIDs soll der Wert
	 * normalisiert werden
	 * 
	 * @param input    input string to normalize
	 * @param category category isni or orchid
	 * @return normalized value depending on isni, orcid pattern
	 */
	private String normalizeByCategory(String input, String category) {
		String stripped = stripDashesAndWhitespaces(input);
		if (stripped.length() != 16) {
			return input;
		} else {
			if (category.equalsIgnoreCase("isni")) {
				return stripped.substring(0, 4) + "-" + stripped.substring(4, 8) + "-" + stripped.substring(8, 12) + "-" + stripped.substring(12, 16);
			} else if (category.equalsIgnoreCase("orcid")) {
				return stripped.substring(0, 4) + " " + stripped.substring(4, 8) + " " + stripped.substring(8, 12) + " " + stripped.substring(12, 16);
			} else {
				return input;
			}
		}
	}

	private String stripDashesAndWhitespaces(String input) {
		return input.replaceAll("-", "").replaceAll("\\s", "");
	}

	/**
	 * isni = custom(de.unituebingen.ub.ubtools.solrmarcMixin.TuelibAuthMixin), getNormalizedValueByTag2(024, "isni")
	 * orcid = custom(de.unituebingen.ub.ubtools.solrmarcMixin.TuelibAuthMixin), getNormalizedValueByTag2(024, "orcid")
	 * 
	 * @param record          implicit call
	 * @param tagNumber       e.g. 024, only use Datafields > 010
	 * @param number2Category isni | orcid
	 * @return
	 */
	public String getNormalizedValueByTag2(final Record record, final String tagNumber, final String number2Category) {

		List<DataField> mainFields = (List<DataField>) (List<?>) record.getVariableFields(tagNumber);
		mainFields.removeIf(m -> m.getSubfield('2') == null);
		mainFields.removeIf(m -> m.getSubfield('a') == null);
		mainFields.removeIf(m -> m.getSubfield('2').getData().equalsIgnoreCase(number2Category) == false);

		if (mainFields.size() == 0) {
			return null;
		} else if (mainFields.size() == 1) {
			return normalizeByCategory(mainFields.get(0).getSubfield('a').getData(), number2Category);
		} else {
			Set<String> differentNormalizedValues = new HashSet<String>();
			for (DataField mainField : mainFields) {
				final String numberA = mainField.getSubfield('a').getData();
				String normalizedValue = normalizeByCategory(numberA, number2Category);
				differentNormalizedValues.add(normalizedValue);
			}
			if (differentNormalizedValues.size() == 1) {
				return differentNormalizedValues.iterator().next();
			} else {
				logger.warning("record id " + record.getControlNumber() + " - multiple field with different content " + number2Category);
				return null;
			}

		}
	}
}
