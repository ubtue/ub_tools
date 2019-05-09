input = ARG1
set terminal pdf
set output ARG2
set autoscale x

# Key means label...
set key outside top right
set xlabel 'Time ('.ARG3.')'
set ylabel 'Memory (MB)'
set title 'Memory Stats'
plot input using 1:($2/(1024)) title 'Free (RAM)' with lines, input using 1:($4/(1024)) title 'Free (Swap)' with lines, input using 1:($3/(1024)) title 'Unevictable' with lines
