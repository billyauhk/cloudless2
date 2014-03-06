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

using namespace cv;
using namespace std;

int main(int argc, char* argv[]){
  GDALDataset *poDataset = NULL;
  char* filename;
  Mat imageBuffer;
  uint64_t xsize, ysize;

// Check arguments
  if(argc<2){
    fprintf(stderr,"Not enough argument!\n");exit(-1);
  }
  filename = argv[1];

// Open file
  GDALAllRegister();
  poDataset = (GDALDataset*) GDALOpen(filename, GA_ReadOnly);
  if(poDataset == NULL){
    fprintf(stderr,"FIle cannot be opened!\n");exit(-1);
  }

// Print some metadata
  printf("Filename: %s\n", argv[1]);
  xsize = poDataset->GetRasterXSize();
  ysize = poDataset->GetRasterYSize();
  printf("Raster Size: %d x %d\n", xsize, ysize);
  printf("Raster Count: %d\n", poDataset->GetRasterCount());

// Read Image data
  imageBuffer.create(ysize, xsize, CV_32FC1);
  poDataset->RasterIO(GF_Read, 0, 0, xsize, ysize,
                      (void*) imageBuffer.ptr(0), xsize, ysize,
                      GDT_Float32, 1, NULL, 0, 0, 0);

// One should be able to get coordinate with GDALApplyGeoTransform() (or maybe also GDALInvGeoTransform())

// Display the data
  imageBuffer.convertTo(imageBuffer, CV_16UC1);
  normalize(imageBuffer, imageBuffer, 0, 65535, NORM_MINMAX);
  namedWindow("Thumbnail",CV_GUI_EXPANDED);
  imshow("Thumbnail", imageBuffer);
  waitKey(0);
  destroyWindow("Thumbnail");

  delete poDataset;
  return 0;
}
