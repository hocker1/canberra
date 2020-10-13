set terminal png size 2800,1080
set key title ""
set xlabel "E [keV]"
set ylabel "N [imp.]"
set grid
set xrange [2000:]
plot input using (9.794*($1)+54.06):2 title "" with lines
