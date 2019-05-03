# Scale font and line width (dpi) by changing the size! It will always display stretched.
set terminal svg size 400,300 enhanced fname 'arial'  fsize 10 butt solid
set output output_file

# Key means label...
set key inside bottom right
set xlabel 'Time'
set ylabel 'Memory (MB)'
set title 'Memory Stats'
plot input_file using 1:($2/(1024 * 1024)) title 'Free (RAM)' with lines, input_file using 1:($4/(1024 * 1024)) title 'Free (Swap)' with linespoints, input_file using 1:($3/(1024 * 1024)) title 'Unevictable' with linespoints
