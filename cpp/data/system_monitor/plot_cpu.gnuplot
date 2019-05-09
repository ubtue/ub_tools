input = ARG1
set terminal pdf
set output ARG2

# Key means label...
set key inside top right
set xlabel 'Time'
set ylabel 'CPU Usage'
set yrange [0:100]
set title 'CPU Stats'
plot input using 1:2 title "Average Usage" with lines
