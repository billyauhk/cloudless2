INC_PATH=-I/usr/include/gdal/ -I/usr/local/include
LD_PATH=-L/usr/lib/

all:landsat7_reader landsat8_reader modis_test

landsat7_reader:landsat7_reader.cpp makefile
	LD_LIBRARY_PATH=${LD_PATH} g++ landsat7_reader.cpp -o landsat7_reader ${INC_PATH} ${LD_PATH} -lgdal1.7.0 `pkg-config opencv --cflags --libs` -fopenmp -lgomp
landsat8_reader:landsat8_reader.cpp makefile
	LD_LIBRARY_PATH=${LD_PATH} g++ landsat8_reader.cpp -o landsat8_reader ${INC_PATH} ${LD_PATH} -lgdal1.7.0 `pkg-config opencv --cflags --libs` -fopenmp -lgomp
modis_test:modis_test.cpp makefile
	LD_LIBRARY_PATH=${LD_PATH} g++ modis_test.cpp -o modis_test ${INC_PATH} ${LD_PATH} -lgdal1.7.0 `pkg-config opencv --cflags --libs` -fopenmp -lgomp
