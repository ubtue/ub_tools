BEGIN { FS = "|" };
{ split($1, s_no_gnd_terms, ";");
  for (i in s_no_gnd_terms) {
       gsub(/[\\.^$(){}\[\]|*+?]/, "\\\\&", s_no_gnd_terms[i])
       $2 = gensub(s_no_gnd_terms[i], "", "g", $2)
       $2 = gensub(/;\s*;/, ";", "g", $2) # Remove potential empty fields
       gsub(/^;/, "", $2) # Remove potential empty fields
       gsub(/;$/, "", $2) # Remove potential empty fields
  }

  if (match($2, /^\s*$/) || match($3, /^\s*$/))
      next
  split($2, stichwort, ";")
  split($3, s_gnd, ";")
  for (j in stichwort) {
       print stichwort[j]" | "s_gnd[j]
  }
}
