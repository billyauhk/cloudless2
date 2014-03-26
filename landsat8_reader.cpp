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

// Constants
#define PI (3.14159265359)

using namespace cv;
using namespace std;

// Metadata file content buffe
// 20k buffer for nominally 7k file should be safe
char fileContent[20000] = {0};

inline double readMeta(const char* itemname){
  char* filePos = NULL;
  filePos = strstr(fileContent, itemname);
  if(filePos==NULL){fprintf(stderr,"Item %s not found!\n", itemname);fflush(stderr);exit(-1);}
  filePos = filePos + strlen(itemname);
  return atof(filePos);
}

int main(int argc, char* argv[]){

// Input filename
  char *basename, *filename;
  uint16_t basenameLength;

// Data to be loaded
  uint16_t numChannel = 11;  // Hard-coded as we know there must be 11 channels
  GDALDataset *poDataset[numChannel]; // Input dataset: 11 channels of Landsat 8
  uint16_t band_number;
  uint64_t xsize, ysize;

// The metafile handle
  FILE* infile;
  char itemname[100];
  uint32_t fileSize = 0;
  double multValue, addValue;

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

// Read metadata file -- read the entire file into memory
  sprintf(filename, "%s_MTL.txt", argv[1]);
  infile = fopen(filename,"r");
  fileSize = fread(fileContent, sizeof(unsigned char), 20000, infile);
  if(fileSize==0){
    fprintf(stderr,"File size error! fread() failed!\n");fflush(stderr);exit(-1);
  }
  printf("Metadata File size: %d\n",fileSize);

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
      printf("Panochromatic Raster Size: %lu x %lu\n", xsize, ysize);
    }
    xsize = poDataset[0]->GetRasterXSize();
    ysize = poDataset[0]->GetRasterYSize();
    printf("Others Raster Size: %lu x %lu\n", xsize, ysize);

  // Read Image data at 30m
  // While skip panochromatic band (otherwise OOM)
    imageBuffer.create(ysize, xsize, CV_32FC1);
    outputBuffer.create(ysize, xsize, CV_32FC(numChannel));
    outputBuffer.zeros(ysize, xsize, CV_32FC(numChannel));

/* Image reading and adjust: Pipeline as follows:
   Part(1): Band-by-band Possible Processing
            DN -> TOA Radiance -> TOA reflectance
   At the time I do not plan on cloud-shadow masking yet...
*/

    for(band_number=1;band_number<=numChannel;band_number++){
      if(band_number!=8){
        fprintf(stderr,"Reading data from band %d...",band_number);
        bandArray[0] = 1;
        imageBuffer.zeros(ysize, xsize, CV_32FC1);
        poDataset[band_number-1]->RasterIO(GF_Read, 0, 0, xsize, ysize,
                                           (void*) imageBuffer.ptr(0), xsize, ysize,
                                           GDT_Float32, 1, bandArray, 0, 0, 0);

  /* Get the TOA reflectance, solar angle adjusted with metadata basename_MTL.txt
     For landsat 7 documentation is at: http://landsathandbook.gsfc.nasa.gov/data_prod/prog_sect11_3.html
     For landsat 8 documentation is at: http://www.yale.edu/ceo/Documentation/Landsat_DN_to_Reflectance.pdf
  */
        sprintf(itemname, "RADIANCE_MULT_BAND_%u = ", band_number);
        multValue = readMeta(itemname);
        sprintf(itemname, "RADIANCE_ADD_BAND_%u = ", band_number);
        addValue = readMeta(itemname);

  // Conversion to TOA Radiance
        imageBuffer = imageBuffer*multValue + addValue;
  // Conversion to TOA Reflectance
        imageBuffer = imageBuffer;
  // Copy to output array
        for(int x=0;x<xsize;x++){
          for(int y=0;y<ysize;y++){
            ((float*)(outputBuffer.data))[(y*xsize+x)*numChannel+(band_number-1)] = ((float*)(imageBuffer.data))[y*xsize+x];
          }
        }
        fprintf(stderr,"...done\n");fflush(stderr);
      }
    }

/* Image masking and spatial processing: Pipeline as follows:
   Part(2): Processing with all bands
            Cloud Masking -> (Cloud-shadow Masking) -> Maskout no data area
            -> De-haze -> White-balance using Cloud -> Clipping -> Channel select
   At the time I do not plan on cloud-shadow masking yet...
   BTW channel 8 (panochromatic) is used as the temporary alpha MASK
*/


/* Image masking and spatial processing: Pipeline as follows:
   Part(3): Zooming, Read Panochromatic -> PANSHARP
            -> Output 4-channel PANSHARP-ed+Alpha 16-bit GeoTIFF?
*/


  // One should be able to get coordinate with GDALApplyGeoTransform() (or maybe also GDALInvGeoTransform())

  // Save using OpenCV's YAML.gz, but is not efficient at all
/*  FileStorage storage("image.yaml.gz", FileStorage::WRITE);
  storage << "data" << outputBuffer;
  storage.release();*/

  // Free the resources
    for(band_number=1;band_number<=numChannel;band_number++){
      delete poDataset[band_number-1];
      poDataset[band_number-1] = NULL;
    }
    free(filename);

  return 0;
}
