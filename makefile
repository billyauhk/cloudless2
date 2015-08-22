INC_PATH=-I/usr/include/gdal/ -I/usr/local/include

all:landsat7_reader landsat8_reader modis_test

landsat7_reader:landsat7_reader.cpp makefile
	g++ landsat7_reader.cpp -o landsat7_reader ${INC_PATH} ${LD_PATH} -lgdal `pkg-config opencv --cflags --libs` -fopenmp -lgomp
landsat8_reader:landsat8_reader.cpp makefile
	g++ landsat8_reader.cpp -o landsat8_reader ${INC_PATH} ${LD_PATH} -lgdal `pkg-config opencv --cflags --libs` -fopenmp -lgomp
modis_test:modis_test.cpp makefile
	g++ modis_test.cpp -o modis_test ${INC_PATH} ${LD_PATH} -lgdal `pkg-config opencv --cflags --libs` -fopenmp -lgomp
