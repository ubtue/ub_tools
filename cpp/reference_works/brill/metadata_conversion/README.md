Contains Scripts for conversion of Brill DC metadata to BSZ-compliant MARC-Records
Use `convert_all_brill_refworks.sh` to convert all archives. Subscripts are called appropriately

For RGG4 the titles have to be replaced.
This is done by associating the titles from the website. Assigment is done automatically for the most part (c.f. `associate_rgg4_titles.sh`). Titles where the association is ambiguous or no web entries exist must be handled separately (c.f. `rgg4_daten/rgg4_multicandidates.txt` and `rgg4_unassociated.txt`). Use `generate_rgg4_multicandidates_rewrite_file.sh` to convert non-ambiguous cases ot a `marc_filter` compatible rewrite file and copy it to `rgg4_daten/rgg4_multicandidates_rewrite.txt`.


