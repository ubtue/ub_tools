
/* We would like to have a tool that extracts translations for given
   authority data
   The german term is found in Field 150 
   Currently there are two different kinds of translations:
   TxTheo-Translations with the following definitions:

   710: Körperschaft - fremdsprachige Äquivalenz        
   711: Konferenz - fremdsprachige Äquivalenz  
   700: Person - fremdsprachige Äquivalenz     
   730: Titel - fremdsprachige Äquivalenz      
   750: Sachbegriff - fremdsprachige Äquivalenz  
   751: Geografikum - fremdsprachige Äquivalenz    

   LoC/Rameau Translations:
   
   700: Person - Bevorzugter Name in einem anderen Datenbestand
   710: Körperschaft - Bevorzugter Name in einem anderen Datenbestand              
   711: Konferenz - Bevorzugte Benennung in einem anderen Datenbestand
   730: Einheitstitel - Bevorzugter Name in einem anderen Datenbestand
   750: Sachbegriff - Bevorzugte Benennung in einem anderen Datenbestand
   751: Geografikum - Bevorzugter Name in einem anderen Datenbestand  
   
*/ 


#include <iostream>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"

// Languages to handle
//enum languages { "en","fr" };

const unsigned int NUMBER_OF_LANGUAGES = 2;
const std::string languages_to_create_str = "en, fr";
enum languages { en, fr };

void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_marc_input extracted_translations\n";
    std::exit(EXIT_FAILURE);
}

void augment_ixtheo_tag_with_language(const MarcUtil::Record &record, const std::string tag, std::vector<std::string> &translations) {
    auto ixtheo_pos = std::find(translations.begin(), translations.end(), "IxTheo");
    if (ixtheo_pos != translations.end()) {
        std::vector<std::string> ixtheo_lang_code;
        record.extractSubfields(tag, "9" , &ixtheo_lang_code);
        for (auto lang_code : ixtheo_lang_code) {
            if (lang_code.find("eng") != std::string::npos && *ixtheo_pos != "IxTheo_eng")
               *ixtheo_pos = *ixtheo_pos + "_eng";
            // FIXME: There are currently no french translations... 
            else if (lang_code.find("fra") != std::string::npos && *ixtheo_pos != "IxTheo_fra")
               *ixtheo_pos = *ixtheo_pos + "_fra";
        }
    }
}


void ExtractTranslations(File* const marc_norm_input, 
                         const std::string german_term_field_spec,
                         const std::string translation_field_spec,
                         std::map<std::string,std::string> term_to_translation_map[]) {

    std::set<std::string> german_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(german_term_field_spec, ':', &german_tags_and_subfield_codes) < 1))
        Error("ExtractTranslations: Need at least one translation field");

    std::set<std::string> translation_tags_and_subfield_codes;
    if (unlikely(StringUtil::Split(translation_field_spec, ':', &translation_tags_and_subfield_codes) < 1))
        Error("ExtractTranslations: Need at least one translation field");

    unsigned count(0);
    
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(marc_norm_input)) {

        std::vector<std::string> german_term;

        // Determine the german term we will have translations for

        for (auto tag_and_subfields : german_tags_and_subfield_codes) {
             const std::string tag(tag_and_subfields.substr(0, 3));
             const std::string subfields(tag_and_subfields.substr(3));
             
             std::vector<std::string> german_term_for_one_field;
             record.extractSubfields(tag, subfields, &german_term_for_one_field);
             
             // We may get the german term from only one field
             if (german_term.size() > 1)  
                 Warning("We have german terms in more than one field for PPN: " + record.getFields()[0]);

             if (not german_term_for_one_field.empty()) 
                 german_term = german_term_for_one_field;
        }        

        std::vector<std::string> all_translations;

        // Extract all additional translations

        for (auto tag_and_subfields : translation_tags_and_subfield_codes) {
            const std::string tag(tag_and_subfields.substr(0, 3));
            const std::string subfields(tag_and_subfields.substr(3));

            std::vector<std::string> translations;
            record.extractSubfields(tag, subfields, &translations);

            // For IxTheo-Translations add the language code in the same field
            augment_ixtheo_tag_with_language(record, tag, translations);
            
            all_translations.insert(std::end(all_translations), std::begin(translations), std::end(translations));
        }
 
        for (auto it = all_translations.begin(); it != all_translations.end(); it++) {
            if (*it == "IxTheo_eng") {
                term_to_translation_map[en].emplace(StringUtil::Join(german_term, ' '), *(it+1));
                ++it;
            }
            else if (*it == "IxTheo_fra") {
                term_to_translation_map[fr].emplace(StringUtil::Join(german_term, ' '), *(it+1));
                ++it;
            }

            else if (*it == "lcsh") {
                term_to_translation_map[en].emplace(StringUtil::Join(german_term, ' '), *(it+1));
                ++it;
            }
            else if (*it == "ram") {
                term_to_translation_map[fr].emplace(StringUtil::Join(german_term, ' '), *(it+1));
                ++it;
            }
            
        }
       
    }

    count++;

}


std::unique_ptr<File> OpenInputFile(const std::string &filename) {
    std::string mode("r");
    mode += MediaTypeUtil::GetFileMediaType(filename) == "application/lz4" ? "u" : "m";
    std::unique_ptr<File> file(new File(filename, mode));
    if (file == nullptr)
        Error("can't open \"" + filename + "\" for reading!");

    return file;
}



int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string norm_data_marc_input_filename(argv[1]);
    std::unique_ptr<File> norm_data_marc_input(OpenInputFile(norm_data_marc_input_filename));

    const std::string extracted_translations_filename(argv[2]);
    if (unlikely(norm_data_marc_input_filename == extracted_translations_filename))
        Error("Norm data input file name equals output file name!");

    std::string output_mode("w");
    if (norm_data_marc_input->isCompressingOrUncompressing())
        output_mode += 'c';
    
    // Create a file for each language
    std::vector<std::string> output_file_components;
    if (unlikely(StringUtil::Split(extracted_translations_filename, ".", &output_file_components) < 1)) 
        Error("extracted_translations_filename " + extracted_translations_filename + " is not valid");
   
    std::vector<std::string> languages_to_create;
    if (unlikely(StringUtil::Split(languages_to_create_str, ',', &languages_to_create) < 1))
        Error("String " + languages_to_create_str + " is not valid");
    
    File* lang_file[NUMBER_OF_LANGUAGES];
    unsigned i(0);
  
    // Derive output components from given input filename
    std::string extension = (output_file_components.size() > 1) ? output_file_components.back() : "";
    std::string basename;
    if (extension != "")
        output_file_components.pop_back();
    basename = StringUtil::Join(output_file_components, ".");
      
    // Assemble output filename
    for (auto lang : languages_to_create) {
         lang = StringUtil::Trim(lang);

         std::string lang_file_name_str = 
             (extension != "") ? basename + "_" + lang + "." + extension : basename + "_" + lang;

         lang_file[i++] = new File(lang_file_name_str, output_mode);
         if (lang_file[i] == NULL)
             Error("can't open \"" + lang_file_name_str + "\" for writing!");
    }

    try {
        std::map<std::string, std::string> term_to_translation_map[NUMBER_OF_LANGUAGES];

        ExtractTranslations(norm_data_marc_input.get(), "100a:150a", "750a2", term_to_translation_map);
        for (auto line : term_to_translation_map[en]) {
              
             *(lang_file[en]) << line.first << "|" << line.second << "\n";
        }

        for (auto line : term_to_translation_map[fr]) {
             *(lang_file[fr]) << line.first << "|" << line.second << "\n";
        }

    } catch (const std::exception &x) {
      Error("caught exception: " + std::string(x.what()));
    }

}


