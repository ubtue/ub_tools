/** \file    HtmlUtil.cc
 *  \brief   Implementation of HTML-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Artur Kedzierski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2016-2020 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "HtmlUtil.h"
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "HtmlParser.h"
#include "StringUtil.h"
#include "TextUtil.h"


namespace HtmlUtil {


bool DecodeEntityLatin1(const char * const entity_string, char * const ch) {
    // numeric entity?
    if (entity_string[0] == '#') { // Yes!
        errno = 0;
        unsigned long code;
        if (entity_string[1] == 'x')
            code = ::strtoul(entity_string + 2, nullptr, 16);
        else
            code = ::strtoul(entity_string + 1, nullptr, 10);
        if (errno != 0)
            return false;

        if (code <= 255) {
            *ch = static_cast<char>(code);
            return true;
        }

        switch (code) {
        case 946:
        case 0xCF90:
            *ch = static_cast<char>(223); // Map the lowercase Greek beta to a German sharp-s.
            return true;
        default:
            return false;
        }
    }

    if (std::strcmp(entity_string, "quot") == 0) {
        *ch = '"';
        return true;
    }

    if (std::strcmp(entity_string, "amp") == 0) {
        *ch = '&';
        return true;
    }

    if (std::strcmp(entity_string, "lt") == 0) {
        *ch = '<';
        return true;
    }

    if (std::strcmp(entity_string, "gt") == 0) {
        *ch = '>';
        return true;
    }

    if (std::strcmp(entity_string, "nbsp") == 0) {
        *ch = ' ';
        return true;
    }

    if (std::strcmp(entity_string, "iexcl") == 0) {
        *ch = static_cast<char>(161);
        return true;
    }

    if (std::strcmp(entity_string, "cent") == 0) {
        *ch = static_cast<char>(162);
        return true;
    }

    if (std::strcmp(entity_string, "pound") == 0) {
        *ch = static_cast<char>(163);
        return true;
    }

    if (std::strcmp(entity_string, "curren") == 0) {
        *ch = static_cast<char>(164);
        return true;
    }

    if (std::strcmp(entity_string, "yen") == 0) {
        *ch = static_cast<char>(165);
        return true;
    }

    if (std::strcmp(entity_string, "brvbar") == 0) {
        *ch = static_cast<char>(166);
        return true;
    }

    if (std::strcmp(entity_string, "sect") == 0) {
        *ch = static_cast<char>(167);
        return true;
    }

    if (std::strcmp(entity_string, "uml") == 0) {
        *ch = static_cast<char>(168);
        return true;
    }

    if (std::strcmp(entity_string, "copy") == 0) {
        *ch = static_cast<char>(169);
        return true;
    }

    if (std::strcmp(entity_string, "ordf") == 0) {
        *ch = static_cast<char>(170);
        return true;
    }

    if (std::strcmp(entity_string, "laquo") == 0) {
        *ch = static_cast<char>(171);
        return true;
    }

    if (std::strcmp(entity_string, "not") == 0) {
        *ch = static_cast<char>(172);
        return true;
    }

    if (std::strcmp(entity_string, "shy") == 0) {
        *ch = static_cast<char>(173);
        return true;
    }

    if (std::strcmp(entity_string, "reg") == 0) {
        *ch = static_cast<char>(174);
        return true;
    }

    if (std::strcmp(entity_string, "macr") == 0) {
        *ch = static_cast<char>(175);
        return true;
    }

    if (std::strcmp(entity_string, "deg") == 0) {
        *ch = static_cast<char>(176);
        return true;
    }

    if (std::strcmp(entity_string, "plusmn") == 0) {
        *ch = static_cast<char>(177);
        return true;
    }

    if (std::strcmp(entity_string, "sup2") == 0) {
        *ch = static_cast<char>(178);
        return true;
    }

    if (std::strcmp(entity_string, "sup3") == 0) {
        *ch = static_cast<char>(179);
        return true;
    }

    if (std::strcmp(entity_string, "acute") == 0) {
        *ch = static_cast<char>(180);
        return true;
    }

    if (std::strcmp(entity_string, "micro") == 0) {
        *ch = static_cast<char>(181);
        return true;
    }

    if (std::strcmp(entity_string, "para") == 0) {
        *ch = static_cast<char>(182);
        return true;
    }

    if (std::strcmp(entity_string, "middot") == 0) {
        *ch = static_cast<char>(183);
        return true;
    }

    if (std::strcmp(entity_string, "cedil") == 0) {
        *ch = static_cast<char>(184);
        return true;
    }

    if (std::strcmp(entity_string, "sup1") == 0) {
        *ch = static_cast<char>(185);
        return true;
    }

    if (std::strcmp(entity_string, "ordm") == 0) {
        *ch = static_cast<char>(186);
        return true;
    }

    if (std::strcmp(entity_string, "raquo") == 0) {
        *ch = static_cast<char>(187);
        return true;
    }

    if (std::strcmp(entity_string, "fraq14") == 0) {
        *ch = static_cast<char>(188);
        return true;
    }

    if (std::strcmp(entity_string, "fraq12") == 0) {
        *ch = static_cast<char>(189);
        return true;
    }

    if (std::strcmp(entity_string, "fraq34") == 0) {
        *ch = static_cast<char>(190);
        return true;
    }

    if (std::strcmp(entity_string, "iquest") == 0) {
        *ch = static_cast<char>(191);
        return true;
    }

    if (std::strcmp(entity_string, "Agrave") == 0) {
        *ch = static_cast<char>(192);
        return true;
    }

    if (std::strcmp(entity_string, "Aacute") == 0) {
        *ch = static_cast<char>(193);
        return true;
    }

    if (std::strcmp(entity_string, "Acirc") == 0) {
        *ch = static_cast<char>(194);
        return true;
    }

    if (std::strcmp(entity_string, "Atilde") == 0) {
        *ch = static_cast<char>(195);
        return true;
    }

    if (std::strcmp(entity_string, "Auml") == 0) {
        *ch = static_cast<char>(196);
        return true;
    }

    if (std::strcmp(entity_string, "Aring") == 0) {
        *ch = static_cast<char>(197);
        return true;
    }

    if (std::strcmp(entity_string, "AElig") == 0) {
        *ch = static_cast<char>(198);
        return true;
    }

    if (std::strcmp(entity_string, "Ccedil") == 0) {
        *ch = static_cast<char>(199);
        return true;
    }

    if (std::strcmp(entity_string, "Egrave") == 0) {
        *ch = static_cast<char>(200);
        return true;
    }

    if (std::strcmp(entity_string, "Eacute") == 0) {
        *ch = static_cast<char>(201);
        return true;
    }

    if (std::strcmp(entity_string, "Ecirc") == 0) {
        *ch = static_cast<char>(202);
        return true;
    }

    if (std::strcmp(entity_string, "Euml") == 0) {
        *ch = static_cast<char>(203);
        return true;
    }

    if (std::strcmp(entity_string, "Igrave") == 0) {
        *ch = static_cast<char>(204);
        return true;
    }

    if (std::strcmp(entity_string, "Iacute") == 0) {
        *ch = static_cast<char>(205);
        return true;
    }

    if (std::strcmp(entity_string, "Icirc") == 0) {
        *ch = static_cast<char>(206);
        return true;
    }

    if (std::strcmp(entity_string, "Iuml") == 0) {
        *ch = static_cast<char>(207);
        return true;
    }

    if (std::strcmp(entity_string, "ETH") == 0) {
        *ch = static_cast<char>(208);
        return true;
    }

    if (std::strcmp(entity_string, "Ntilde") == 0) {
        *ch = static_cast<char>(209);
        return true;
    }

    if (std::strcmp(entity_string, "Ograve") == 0) {
        *ch = static_cast<char>(210);
        return true;
    }

    if (std::strcmp(entity_string, "Oacute") == 0) {
        *ch = static_cast<char>(211);
        return true;
    }

    if (std::strcmp(entity_string, "Ocirc") == 0) {
        *ch = static_cast<char>(212);
        return true;
    }

    if (std::strcmp(entity_string, "Otilde") == 0) {
        *ch = static_cast<char>(213);
        return true;
    }

    if (std::strcmp(entity_string, "Ouml") == 0) {
        *ch = static_cast<char>(214);
        return true;
    }

    if (std::strcmp(entity_string, "times") == 0) {
        *ch = static_cast<char>(215);
        return true;
    }

    if (std::strcmp(entity_string, "Oslash") == 0) {
        *ch = static_cast<char>(216);
        return true;
    }

    if (std::strcmp(entity_string, "Ugrave") == 0) {
        *ch = static_cast<char>(217);
        return true;
    }

    if (std::strcmp(entity_string, "Uacute") == 0) {
        *ch = static_cast<char>(218);
        return true;
    }

    if (std::strcmp(entity_string, "Ucirc") == 0) {
        *ch = static_cast<char>(219);
        return true;
    }

    if (std::strcmp(entity_string, "Uuml") == 0) {
        *ch = static_cast<char>(220);
        return true;
    }

    if (std::strcmp(entity_string, "Yacute") == 0) {
        *ch = static_cast<char>(221);
        return true;
    }

    if (std::strcmp(entity_string, "THORN") == 0) {
        *ch = static_cast<char>(222);
        return true;
    }

    if (std::strcmp(entity_string, "szlig") == 0) {
        *ch = static_cast<char>(223);
        return true;
    }

    if (std::strcmp(entity_string, "beta") == 0) {
        *ch = static_cast<char>(223);
        return true;
    }

    if (std::strcmp(entity_string, "agrave") == 0) {
        *ch = static_cast<char>(224);
        return true;
    }

    if (std::strcmp(entity_string, "aacute") == 0) {
        *ch = static_cast<char>(225);
        return true;
    }

    if (std::strcmp(entity_string, "acirc") == 0) {
        *ch = static_cast<char>(226);
        return true;
    }

    if (std::strcmp(entity_string, "atilde") == 0) {
        *ch = static_cast<char>(227);
        return true;
    }

    if (std::strcmp(entity_string, "auml") == 0) {
        *ch = static_cast<char>(228);
        return true;
    }

    if (std::strcmp(entity_string, "aring") == 0) {
        *ch = static_cast<char>(229);
        return true;
    }

    if (std::strcmp(entity_string, "aelig") == 0) {
        *ch = static_cast<char>(230);
        return true;
    }

    if (std::strcmp(entity_string, "ccedil") == 0) {
        *ch = static_cast<char>(231);
        return true;
    }

    if (std::strcmp(entity_string, "egrave") == 0) {
        *ch = static_cast<char>(232);
        return true;
    }

    if (std::strcmp(entity_string, "eacute") == 0) {
        *ch = static_cast<char>(233);
        return true;
    }

    if (std::strcmp(entity_string, "ecirc") == 0) {
        *ch = static_cast<char>(234);
        return true;
    }

    if (std::strcmp(entity_string, "euml") == 0) {
        *ch = static_cast<char>(235);
        return true;
    }

    if (std::strcmp(entity_string, "igrave") == 0) {
        *ch = static_cast<char>(236);
        return true;
    }

    if (std::strcmp(entity_string, "iacute") == 0) {
        *ch = static_cast<char>(237);
        return true;
    }

    if (std::strcmp(entity_string, "icirc") == 0) {
        *ch = static_cast<char>(238);
        return true;
    }

    if (std::strcmp(entity_string, "iuml") == 0) {
        *ch = static_cast<char>(239);
        return true;
    }

    if (std::strcmp(entity_string, "eth") == 0) {
        *ch = static_cast<char>(240);
        return true;
    }

    if (std::strcmp(entity_string, "ntilde") == 0) {
        *ch = static_cast<char>(241);
        return true;
    }

    if (std::strcmp(entity_string, "ograve") == 0) {
        *ch = static_cast<char>(242);
        return true;
    }

    if (std::strcmp(entity_string, "oacute") == 0) {
        *ch = static_cast<char>(243);
        return true;
    }

    if (std::strcmp(entity_string, "ocirc") == 0) {
        *ch = static_cast<char>(244);
        return true;
    }

    if (std::strcmp(entity_string, "otilde") == 0) {
        *ch = static_cast<char>(245);
        return true;
    }

    if (std::strcmp(entity_string, "ouml") == 0) {
        *ch = static_cast<char>(246);
        return true;
    }

    if (std::strcmp(entity_string, "divide") == 0) {
        *ch = static_cast<char>(247);
        return true;
    }

    if (std::strcmp(entity_string, "oslash") == 0) {
        *ch = static_cast<char>(248);
        return true;
    }

    if (std::strcmp(entity_string, "ugrave") == 0) {
        *ch = static_cast<char>(249);
        return true;
    }

    if (std::strcmp(entity_string, "uacute") == 0) {
        *ch = static_cast<char>(250);
        return true;
    }

    if (std::strcmp(entity_string, "ucirc") == 0) {
        *ch = static_cast<char>(251);
        return true;
    }

    if (std::strcmp(entity_string, "uuml") == 0) {
        *ch = static_cast<char>(252);
        return true;
    }

    if (std::strcmp(entity_string, "yacute") == 0) {
        *ch = static_cast<char>(253);
        return true;
    }

    if (std::strcmp(entity_string, "thorn") == 0) {
        *ch = static_cast<char>(254);
        return true;
    }

    if (std::strcmp(entity_string, "yuml") == 0) {
        *ch = static_cast<char>(255);
        return true;
    }

    *ch = '\0';
    return false;
}


std::string &ReplaceEntitiesLatin1(std::string * const s, const UnknownEntityMode unknown_entity_mode) {
    std::string result;
    std::string::const_iterator ch(s->begin());
    while (ch != s->end()) {
        if (likely(*ch != '&')) {
            // A non-entity character:
            result += *ch;
            ++ch;
        } else {
            // The start of an entity:
            ++ch;

            // Read the entity:
            std::string entity;
            while (ch != s->end() and *ch != ';' and *ch != '&') {
                entity += *ch;
                ++ch;
            }

            // Output the entity:
            char decoded_char;
            if (DecodeEntityLatin1(entity, &decoded_char))
                result += decoded_char;
            else if (unknown_entity_mode == DELETE_UNKNOWN_ENTITIES)
                result += "&" + entity + ";";

            // Advance to next letter:
            if (ch != s->end() and *ch != '&')
                ++ch;
        }
    }

    return *s = result;
}


const std::unordered_map<std::string, std::string> entities_to_utf8_map{
    { "lt", "<" },     { "gt", ">" },      { "amp", "&" },    { "quot", "\"" },   { "Agrave", "À" },   { "Aacute", "Á" },
    { "Acirc", "Â" },  { "Atilde", "Ã" },  { "Auml", "Ä" },   { "Aring", "Å" },   { "AElig", "Æ" },    { "Ccedil", "Ç" },
    { "Egrave", "È" }, { "Eacute", "É" },  { "Ecirc", "Ê" },  { "Euml", "Ë" },    { "Igrave", "Ì" },   { "Iacute", "Í" },
    { "Icirc", "Î" },  { "Iuml", "Ï" },    { "ETH", "Ð" },    { "Ntilde", "Ñ" },  { "Ograve", "Ò" },   { "Oacute", "Ó" },
    { "Ocirc", "Ô" },  { "Otilde", "Õ" },  { "Ouml", "Ö" },   { "Oslash", "Ø" },  { "Ugrave", "Ù" },   { "Uacute", "Ú" },
    { "Ucirc", "Û" },  { "Uuml", "Ü" },    { "Yacute", "Ý" }, { "THORN", "Þ" },   { "szlig", "ß" },    { "agrave", "à" },
    { "aacute", "á" }, { "acirc", "â" },   { "atilde", "ã" }, { "auml", "ä" },    { "aring", "å" },    { "aelig", "æ" },
    { "ccedil", "ç" }, { "egrave", "è" },  { "eacute", "é" }, { "ecirc", "ê" },   { "euml", "ë" },     { "igrave", "ì" },
    { "iacute", "í" }, { "icirc", "î" },   { "iuml", "ï" },   { "eth", "ð" },     { "ntilde", "ñ" },   { "ograve", "ò" },
    { "oacute", "ó" }, { "ocirc", "ô" },   { "otilde", "õ" }, { "ouml", "ö" },    { "oslash", "ø" },   { "ugrave", "ù" },
    { "uacute", "ú" }, { "ucirc", "û" },   { "uuml", "ü" },   { "yacute", "ý" },  { "thorn", "þ" },    { "yuml", "ÿ" },
    { "nbsp", " " },   { "iexcl", "¡" },   { "cent", "¢" },   { "pound", "£" },   { "curren", "¤" },   { "yen", "¥" },
    { "brvbar", "¦" }, { "sect", "§" },    { "uml", "¨" },    { "copy", "©" },    { "ordf", "ª" },     { "laquo", "«" },
    { "not", "¬" },    { "shy", "­" },     { "reg", "®" },    { "macr", "¯" },    { "deg", "°" },      { "plusmn", "±" },
    { "sup2", "²" },   { "sup3", "³" },    { "acute", "´" },  { "micro", "µ" },   { "para", "¶" },     { "cedil", "¸" },
    { "sup1", "¹" },   { "ordm", "º" },    { "raquo", "»" },  { "frac14", "¼" },  { "frac12", "½" },   { "frac34", "¾" },
    { "iquest", "¿" }, { "times", "×" },   { "divide", "÷" }, { "forall", "∀" },  { "part", "∂" },     { "exist", "∃" },
    { "empty", "∅" },  { "nabla", "∇" },   { "isin", "∈" },   { "notin", "∉" },   { "ni", "∋" },       { "prod", "∏" },
    { "sum", "∑" },    { "minus", "−" },   { "lowast", "∗" }, { "radic", "√" },   { "prop", "∝" },     { "infin", "∞" },
    { "ang", "∠" },    { "and", "∧" },     { "or", "∨" },     { "cap", "∩" },     { "cup", "∪" },      { "int", "∫" },
    { "there4", "∴" }, { "sim", "∼" },     { "cong", "≅" },   { "asymp", "≈" },   { "ne", "≠" },       { "equiv", "≡" },
    { "le", "≤" },     { "ge", "≥" },      { "sub", "⊂" },    { "sup", "⊃" },     { "nsub", "⊄" },     { "supe", "⊇" },
    { "oplus", "⊕" },  { "otimes", "⊗" },  { "perp", "⊥" },   { "sdot", "⋅" },    { "Beta", "Β" },     { "Gamma", "Γ" },
    { "Delta", "Δ" },  { "Epsilon", "Ε" }, { "Zeta", "Ζ" },   { "Eta", "Η" },     { "Iota", "Ι" },     { "Kappa", "Κ" },
    { "Mu", "Μ" },     { "Nu", "Ν" },      { "Xi", "Ξ" },     { "Omicron", "Ο" }, { "Rho", "Ρ" },      { "Sigma", "Σ" },
    { "Tau", "Τ" },    { "Upsilon", "Υ" }, { "Phi", "Φ" },    { "Chi", "Χ" },     { "Psi", "Ψ" },      { "Omega", "Ω" },
    { "alpha", "α" },  { "beta", "β" },    { "gamma", "γ" },  { "delta", "δ" },   { "epsilon", "ε" },  { "eta", "η" },
    { "theta", "θ" },  { "iota", "ι" },    { "kappa", "κ" },  { "lambda", "λ" },  { "mu", "μ" },       { "nu", "ν" },
    { "xi", "ξ" },     { "omicron", "ο" }, { "rho", "ρ" },    { "sigmaf", "ς" },  { "sigma", "σ" },    { "upsilon", "υ" },
    { "phi", "φ" },    { "chi", "χ" },     { "psi", "ψ" },    { "omega", "ω" },   { "thetasym", "ϑ" }, { "upsih", "ϒ" },
    { "piv", "ϖ" },    { "OElig", "Œ" },   { "oelig", "œ" },  { "Scaron", "Š" },  { "scaron", "š" },   { "Yuml", "Ÿ" },
    { "fnof", "ƒ" },   { "circ", "ˆ" },    { "tilde", "˜" },  { "ensp", " " },    { "emsp", " " },     { "thinsp", " " },
    { "zwnj", "‌" }, { "zwj", "‍" },   { "lrm", "‎" },  { "rlm", "‏" },   { "ndash", "–" },    { "mdash", "—" },
    { "lsquo", "‘" },  { "rsquo", "’" },   { "sbquo", "‚" },  { "ldquo", "“" },   { "rdquo", "”" },    { "bdquo", "„" },
    { "dagger", "†" }, { "Dagger", "‡" },  { "bull", "•" },   { "hellip", "…" },  { "permil", "‰" },   { "prime", "′" },
    { "Prime", "″" },  { "lsaquo", "‹" },  { "rsaquo", "›" }, { "oline", "‾" },   { "euro", "€" },     { "trade", "™" },
    { "larr", "←" },   { "uarr", "↑" },    { "rarr", "→" },   { "darr", "↓" },    { "harr", "↔" },     { "crarr", "↵" },
    { "lceil", "⌈" },  { "rceil", "⌉" },   { "lfloor", "⌊" }, { "rfloor", "⌋" },  { "loz", "◊" },      { "spades", "♠" },
    { "clubs", "♣" },  { "hearts", "♥" },  { "diams", "♦" },
};


static bool DecodeEntityUTF8(const std::string &entity_string, std::string * const s) {
    // numeric entity?
    if (entity_string[0] == '#') { // Yes!
        uint32_t code_point;
        errno = 0;
        if (entity_string[1] == 'x')
            code_point = ::strtoul(entity_string.c_str() + 2, nullptr, 16);
        else
            code_point = ::strtoul(entity_string.c_str() + 1, nullptr, 10);
        if (errno != 0)
            return false;

        const auto utf8_char(TextUtil::UTF32ToUTF8(code_point));
        s->append(utf8_char);
    } else {
        const auto entity_and_utf8(entities_to_utf8_map.find(entity_string));
        if (unlikely(entity_and_utf8 == entities_to_utf8_map.cend()))
            return false;

        s->append(entity_and_utf8->second);
    }

    return true;
}


static inline bool IsValidEntityCharacter(const char ch) {
    return StringUtil::IsAsciiLetter(ch) or StringUtil::IsDigit(ch) or ch == '#';
}


std::string &ReplaceEntitiesUTF8(std::string * const s, const UnknownEntityMode unknown_entity_mode) {
    std::string result;
    std::string::const_iterator ch(s->begin());
    while (ch != s->end()) {
        if (likely(*ch != '&')) {
            // A non-entity character:
            result += *ch;
            ++ch;
        } else {  // Possibly the start of an entity!
            ++ch; // Skip over the ampersand.

            // Read the entity:
            std::string entity;
            while (ch != s->end() and IsValidEntityCharacter(*ch)) {
                entity += *ch;
                ++ch;
            }

            // Invalid end of entity candidate?
            if (ch == s->end() or *ch != ';') {
                if (unknown_entity_mode == PASS_THROUGH_UNKNOWN_ENTITIES) {
                    result += '&';
                    result += entity;
                }
                continue;
            }

            // Output the entity:
            if (not DecodeEntityUTF8(entity, &result)) {
                if (unknown_entity_mode == PASS_THROUGH_UNKNOWN_ENTITIES) {
                    result += '&';
                    result += entity;
                    result += ';';
                    ++ch; // Skip over the semicolon.
                } else
                    result += "�";
            } else
                ++ch; // Skip over the semicolom at the end of the entity.
        }
    }

    s->swap(result);
    return *s;
}


std::string HtmlEscape(const std::string &unescaped_text) {
    std::string escaped_text;
    escaped_text.reserve(unescaped_text.length());

    for (const char ch : unescaped_text) {
        if (ch == '&')
            escaped_text += "&amp;";
        else if (ch == '<')
            escaped_text += "&lt;";
        else if (ch == '>')
            escaped_text += "&gt;";
        else if (ch == '"')
            escaped_text += "&quot;";
        else
            escaped_text += ch;
    }

    return escaped_text;
}


// IsHtmlEscaped --  Are all '&' and '<' and '>' and quotes escaped?
//
bool IsHtmlEscaped(const std::string &raw_text) {
    for (std::string::const_iterator ch(raw_text.begin()); ch != raw_text.end(); ++ch) {
        if (*ch == '&') {
            ++ch;
            std::string possible_entity;
            const unsigned MAX_ENTITY_NAME_LENGTH(6);
            for (unsigned char_count(0); ch != raw_text.end() and *ch != ';' and char_count < MAX_ENTITY_NAME_LENGTH + 1;
                 ++ch, ++char_count)
                possible_entity += *ch;
            if (ch == raw_text.end() or *ch != ';') // No entity!
                return false;

            std::string dummy;
            if (not DecodeEntityUTF8(possible_entity, &dummy))
                return false; // Not an entity!
        } else if (*ch == '<' or *ch == '>' or *ch == '"' or *ch == '\'')
            return false;
    }

    // If we make it this far, everything is okay:
    return true;
}


size_t ExtractAllLinks(const std::string &html_document, std::vector<std::string> * const urls) {
    std::list<UrlExtractorParser::UrlAndAnchorText> urls_and_anchor_texts;
    std::string base_url;
    UrlExtractorParser parser(html_document, /* accept_frame_tags = */ true, /* ignore_image_tags = */ false,
                              /* clean_up_anchor_text = */ true, &urls_and_anchor_texts, &base_url);
    parser.parse();

    urls->clear();
    for (const auto &url_and_anchor_text : urls_and_anchor_texts)
        urls->emplace_back(url_and_anchor_text.url_);

    return urls->size();
}


const char NUL('\0');


std::string StripHtmlTags(const std::string &text_with_optional_tags, const bool replace_entities) {
    std::string stripped_text;
    stripped_text.reserve(text_with_optional_tags.size());

    bool in_tag(false);
    char quote(NUL);
    for (auto ch(text_with_optional_tags.cbegin()); ch != text_with_optional_tags.cend(); ++ch) {
        if (quote != NUL) {
            if (*ch == quote)
                quote = NUL;
        } else if (in_tag) {
            if (*ch == '>') {
                in_tag = false;
                stripped_text += ' ';
            } else if (*ch == '\'' or *ch == '"')
                quote = *ch;
        } else if (*ch == '<' and (ch + 1) != text_with_optional_tags.cend() and (StringUtil::IsAsciiLetter(*(ch + 1)) or *(ch + 1) == '/'))
            in_tag = true;
        else
            stripped_text += *ch;
    }

    if (replace_entities)
        ReplaceEntitiesUTF8(&stripped_text);

    return TextUtil::CollapseAndTrimWhitespace(&stripped_text);
}


std::string ShortenText(const std::string &html_text, const size_t max_length) {
    std::string shortened_text;
    shortened_text.reserve(html_text.size());

    bool in_tag(false);
    size_t length(0);

    static const std::string placeholder("...");
    unsigned max_length_without_placeholder(max_length - placeholder.length());
    for (const auto ch : html_text) {
        if (ch == '<')
            in_tag = true;
        else if (ch == '>')
            in_tag = false;
        else if (not in_tag and ch != '\r' and ch != '\n' and ch != '\t') {
            if (not TextUtil::IsUFT8ContinuationByte(ch))
                ++length;

            if (length >= max_length_without_placeholder) {
                if (length == max_length_without_placeholder)
                    shortened_text += placeholder;
                continue;
            }
        }
        shortened_text += ch;
    }
    return shortened_text;
}


} // namespace HtmlUtil
