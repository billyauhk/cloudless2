/*
Landsat 8 GeoTIFF reader. Use GDAL to extract the UInt16 as Float32, and put into OpenCV Mat
Then we could use OpenCV-based method to compute the necessary stuff.
And, at last, write out pixels with valid observation into a file...spatialLite/rasterlite?
*/

// Headers for GDAL
#include <gdal_priv.h>
#include "cpl_conv.h"
// Headers for OpenCV
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv2/opencv.hpp>
// Other general headers
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

using namespace cv;
using namespace std;

int main(int argc, char* argv[]){
  GDALDataset *poDataset = NULL;
  Mat imageBuffer, outputBuffer;
  double pixel2geo[6];
  double geo2pixel[6];
  uint64_t xsize, ysize;
  char GIBS_XML[1000];
  char outFileName[100];
  int year, month, day;
  int numChannel;

  // No Check arguments
     year = atoi(argv[2]);
    month = atoi(argv[3]);
      day = atoi(argv[4]);
  // Prepare the GDAL driver
    GDALAllRegister();

  // Get the data

    sprintf(GIBS_XML, 
    "<GDAL_WMS>" \
      "<Service name=\"TMS\">" \
        "<ServerUrl>http://map1.vis.earthdata.nasa.gov/wmts-geo/MODIS_%s_CorrectedReflectance_TrueColor/default/%04d-%02d-%02d/EPSG4326_250m/${z}/${y}/${x}.jpg</ServerUrl>" \
      "</Service>" \
      "<DataWindow>" \
        "<UpperLeftX>-180.0</UpperLeftX>" \
        "<UpperLeftY>90</UpperLeftY>" \
        "<LowerRightX>396.0</LowerRightX>" \
        "<LowerRightY>-198</LowerRightY>" \
        "<TileLevel>1</TileLevel>" \
        "<TileCountX>2</TileCountX>" \
        "<TileCountY>1</TileCountY>" \
        "<YOrigin>top</YOrigin>" \
      "</DataWindow>" \
      "<Projection>EPSG:4326</Projection>" \
      "<BlockSizeX>512</BlockSizeX>" \
      "<BlockSizeY>512</BlockSizeY>" \
      "<BandsCount>3</BandsCount>" \
    "</GDAL_WMS>", argv[1], year, month, day);

  // Open file
    poDataset = (GDALDataset*) GDALOpen(GIBS_XML, GA_ReadOnly);
    if(!poDataset){
      fprintf(stderr,"File cannot be opened!\n");exit(-1);
    }

  // Print some metadata
    xsize = poDataset->GetRasterXSize();
    ysize = poDataset->GetRasterYSize();
    printf("Raster Size: %d x %d\n", xsize, ysize);
    numChannel = poDataset->GetRasterCount();
    printf("Raster Count: %d\n", numChannel);

  // Calculate coordinates
    poDataset->GetGeoTransform(pixel2geo);
    GDALInvGeoTransform(pixel2geo, geo2pixel);

    double lat_offset, lon_offset;
    GDALApplyGeoTransform(geo2pixel, 180.0, -90, &lon_offset, &lat_offset);
    printf("Offset=(%lf,%lf)\n",lon_offset,lat_offset);

  // Read Image data
    ysize = (int) lat_offset;
    xsize = (int) lon_offset;

    imageBuffer.create(ysize, xsize, CV_8UC1);
    outputBuffer.create(ysize, xsize, CV_8UC3);
    outputBuffer.zeros(ysize, xsize, CV_8U);
    int bandArray[1];
    for(int i=0;i<3;i++){
      bandArray[0] = i+1;
      imageBuffer.zeros(ysize, xsize, CV_8U);
      poDataset->RasterIO(GF_Read, 0, 0, xsize, ysize,
                          (void*) imageBuffer.ptr(0), xsize, ysize,
                          GDT_Byte, 1, bandArray, 0, 0, 0);
      for(int x=0;x<xsize;x++){
        for(int y=0;y<ysize;y++){
            ((uint8_t*)(outputBuffer.data))[(y*xsize+x)*numChannel+(numChannel-i-1)] = ((uint8_t*)(imageBuffer.data))[y*xsize+x];
        }
      }
    }

    sprintf(outFileName, "%04d-%02d-%02d.png", year, month, day);
    imwrite(outFileName, outputBuffer);

    return 0;
}
