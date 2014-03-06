INC_PATH=-I/usr/include/gdal/ -I/usr/local/include
LD_PATH=-L/usr/lib/

all:landsat8_reader

landsat8_reader:landsat8_reader.cpp makefile
	LD_LIBRARY_PATH=${LD_PATH} g++ landsat8_reader.cpp -o landsat8_reader ${INC_PATH} ${LD_PATH} -lgdal1.7.0 `pkg-config opencv --cflags --libs` -fopenmp -lgomp
