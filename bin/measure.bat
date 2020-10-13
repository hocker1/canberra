echo off

set DATAPATH=..\data
set MODE="baud=9600 parity=n data=8 stop=1"
set COM=COM1

set /p filename="Zadej nazev souboru (bez pripony): "
set filename_raw=%DATAPATH%\%filename%.raw
set filename_dat=%DATAPATH%\%filename%.csv
set filename_png=%DATAPATH%\%filename%.png

IF EXIST %filename_dat% (

	echo Soubor uz existuje.
	
) ELSE (

	canberra.exe %COM% %MODE% %DATAPATH%\%filename%
	gnuplot -e "input='%filename_dat%'" measure.gnu > %filename_png%
)

echo Prekresluji obrazek.
gnuplot -e "input='%filename_dat%'" measure.gnu > %filename_png%
pause
