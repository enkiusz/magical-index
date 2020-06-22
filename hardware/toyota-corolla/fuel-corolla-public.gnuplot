set terminal png size 800,600
set output 'fuel-corolla-public-metric.png'

set xlabel "Avg. speed [km/h]"
set ylabel "Fuel consumption [l/100 km]"
plot "fuel-corolla.dat" using 2:3 title "Toyota Corolla E12T (1.4L I4 1ND-TV D-4D engine)"

set terminal png size 800,600
set output 'fuel-corolla-public-mpg.png'

# Reference: https://en.wikipedia.org/wiki/Miles_per_hour
# 1 km = 0.621371 miles
miles_per_km = 0.621371
# Reference: https://en.wikipedia.org/wiki/Gallon
# 1 galon = 3.785 liters
galons_per_liter = 1.0/3.785

set xlabel "Avg. speed [mph]"
set ylabel "Fuel consumption [mpg]"
plot "fuel-corolla.dat" using ($2 * miles_per_km):((100/$3) * (miles_per_km/galons_per_liter)) title "Toyota Corolla E12T (1.4L I4 1ND-TV D-4D engine)"
