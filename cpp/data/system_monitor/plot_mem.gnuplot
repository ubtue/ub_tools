input = ARG1
set terminal pdf
set output ARG2

# Key means label...
set key outside top right
set xlabel 'Time'
set ylabel 'Memory (MB)'
set title 'Memory Stats'
plot input using 1:($2/(1024)) title 'Free (RAM)' with lines, input using 1:($4/(1024)) title 'Free (Swap)' with lines, input using 1:($3/(1024)) title 'Unevictable' with lines
