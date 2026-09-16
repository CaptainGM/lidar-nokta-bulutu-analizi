set terminal png size 1200,900
set output 'lidar_plot.png'
set size ratio -1
set grid
set xlabel 'X (metre)'
set ylabel 'Y (metre)'
set title 'LIDAR Verisi - Dogru Tespiti ve Kesisimler'
plot 'points.dat' with points pt 7 ps 0.3 lc rgb 'gray' title 'Tum Noktalar', 'line_0.dat' with points pt 7 ps 0.7 title 'Dogru 1', 'line_1.dat' with points pt 7 ps 0.7 title 'Dogru 2', 'line_2.dat' with points pt 7 ps 0.7 title 'Dogru 3', 'line_3.dat' with points pt 7 ps 0.7 title 'Dogru 4', 'line_4.dat' with points pt 7 ps 0.7 title 'Dogru 5', 'line_5.dat' with points pt 7 ps 0.7 title 'Dogru 6', 'line_6.dat' with points pt 7 ps 0.7 title 'Dogru 7', 'line_7.dat' with points pt 7 ps 0.7 title 'Dogru 8', 'line_8.dat' with points pt 7 ps 0.7 title 'Dogru 9', 'line_9.dat' with points pt 7 ps 0.7 title 'Dogru 10', 'line_10.dat' with points pt 7 ps 0.7 title 'Dogru 11', 'line_11.dat' with points pt 7 ps 0.7 title 'Dogru 12', '-' with points pt 5 ps 2 lc rgb 'red' title 'Robot', '-' with points pt 4 ps 2 lc rgb 'blue' title 'Kesisim (>=60 derece)'
0 0
e
1.514863 0.633366
-0.205788 1.982256
-0.254703 2.020602
-0.594577 1.326575
-1.531433 1.402994
-1.168546 1.373394
-0.739254 1.338377
-0.723395 1.337083
1.382513 0.481339
0.217291 2.708936
-0.221966 -1.361675
-0.406310 -1.573426
0.182634 -0.896924
-1.474640 -0.157624
-0.844711 0.904732
-1.639599 0.000932
-1.445415 0.284121
-0.924233 1.044189
e
