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

// Input filename
  char *basename, *filename;
  uint16_t basenameLength;

// Data to be loaded
  uint16_t numChannel = 11;  // Hard-coded as we know there must be 11 channels
  GDALDataset *poDataset[numChannel]; // Input dataset: 11 channels of Landsat 8
  uint16_t band_number;
  uint64_t xsize, ysize;

// Output buffer using OpenCV facility  
  int bandArray[1];
  Mat imageBuffer;
  Mat outputBuffer;
  
// Check arguments
  if(argc<2){
    fprintf(stderr,"Not enough argument!\n");exit(-1);
  }
  basename = argv[1];
  basenameLength = strlen(basename);
  filename = (char*) malloc(sizeof(char)*(basenameLength+10));
  if(!filename){
    fprintf(stderr,"Cannot allocate memory!\n");exit(-1);
  }

// Prepare the GDAL driver
  GDALAllRegister();

  for(band_number=1;band_number<=numChannel;band_number++){
    // Construct file name
    strcpy(filename, basename);
    sprintf(filename+basenameLength, "_B%d.TIF", band_number);

    // Open file
      poDataset[band_number-1] = (GDALDataset*) GDALOpen(filename, GA_ReadOnly);
      if(!poDataset[band_number-1]){
        fprintf(stderr,"File cannot be opened!\n");exit(-1);
      }else{
        fprintf(stderr,"File %s opened.\n", filename);
      }
  }

  // Print some metadata
    printf("Basename: %s\n", basename);
    // Get the size of the panochromatic image
    if(numChannel>=8){
      xsize = poDataset[8-1]->GetRasterXSize();
      ysize = poDataset[8-1]->GetRasterYSize();
      printf("Panochromatic Raster Size: %d x %d\n", xsize, ysize);
    }
    xsize = poDataset[0]->GetRasterXSize();
    ysize = poDataset[0]->GetRasterYSize();
    printf("Others Raster Size: %d x %d\n", xsize, ysize);

  // Read Image data, skip panochromatic band (otherwise OOM)
    imageBuffer.create(ysize, xsize, CV_32FC1);
    outputBuffer.create(ysize, xsize, CV_32FC(numChannel));
    outputBuffer.zeros(ysize, xsize, CV_32FC(numChannel));

    for(band_number=1;band_number<=numChannel;band_number++){
      if(band_number!=8){
        fprintf(stderr,"Reading data from band %d...",band_number);
        bandArray[0] = 1;
        imageBuffer.zeros(ysize, xsize, CV_32FC1);
        poDataset[band_number-1]->RasterIO(GF_Read, 0, 0, xsize, ysize,
                                           (void*) imageBuffer.ptr(0), xsize, ysize,
                                           GDT_Float32, 1, bandArray, 0, 0, 0);
        for(int x=0;x<xsize;x++){
          for(int y=0;y<ysize;y++){
            ((float*)(outputBuffer.data))[(y*xsize+x)*numChannel+(band_number-1)] = ((float*)(imageBuffer.data))[y*xsize+x];
          }
        }
        fprintf(stderr,"...done\n");fflush(stderr);
      }
    }

  // One should be able to get coordinate with GDALApplyGeoTransform() (or maybe also GDALInvGeoTransform())

  // Adjust the radiance as told by the metadata file

  // Display the data if it is not band 8 (panochromatic band takes too much RAM)

  // Free the resources
    for(band_number=1;band_number<=numChannel;band_number++){
      delete poDataset[band_number-1];
      poDataset[band_number-1] = NULL;
    }
    free(filename);

  return 0;
}
