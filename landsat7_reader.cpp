/*
This code is modified from the Landsat 8 program to make it workable with Landsat 7 data on 08May2014.

Landsat 7 GeoTIFF reader. Use GDAL to extract the UInt16 as Float32, and put into OpenCV Mat
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

#include <omp.h>

// Constants
#define PI (3.14159265359)

using namespace cv;
using namespace std;

// Metadata file content buffer
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
  // RGB(321) + Panochromatic(8)
  GDALDataset *poDataset[9];
  uint16_t band;
  uint64_t xsize, ysize;

  // The metafile handle
  FILE* infile;
  char itemname[100];
  uint32_t fileSize = 0;
  double multValue, addValue;

  // Output buffer using OpenCV facility
  int bandArray[1];
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

  for(band=1;band<=9;band++){
    // Construct file name
    switch(band){
      case 1: case 2: case 3: case 4: case 5:
        sprintf(filename, "%s_B%u.TIF", basename, band);break;
      case 6:
        sprintf(filename, "%s_B6_VCID_1.TIF", basename);break;
      case 7:
        sprintf(filename, "%s_B6_VCID_2.TIF", basename);break;
      case 8: case 9:
        sprintf(filename, "%s_B%u.TIF", basename, band-1);break;
      default:
        fprintf(stderr,"Unexpected band value!\n");break;
    }

    // Open file
    poDataset[band-1] = (GDALDataset*) GDALOpen(filename, GA_ReadOnly);
    if(!poDataset[band-1]){
      fprintf(stderr,"File cannot be opened!\n");exit(-1);
    }else{
      fprintf(stderr,"File %s opened.\n", filename);
    }
  }

  // Print some metadata
  printf("Basename: %s\n", basename);
  // Get the size of the panochromatic image
  xsize = poDataset[8]->GetRasterXSize();
  ysize = poDataset[8]->GetRasterYSize();
  printf("Panochromatic Raster Size: %lu x %lu\n", xsize, ysize);
  xsize = poDataset[0]->GetRasterXSize();
  ysize = poDataset[0]->GetRasterYSize();
  printf("Others Raster Size: %lu x %lu\n", xsize, ysize);

  // Read Image data at 30m
  // While skip panochromatic band (otherwise OOM)
  outputBuffer.create(ysize, xsize, CV_32FC3);
  outputBuffer.zeros(ysize, xsize, CV_32FC3);

/* Image reading and adjust: Pipeline as follows:
   Part(1): Band-by-band Possible Processing
            DN -> TOA Radiance -> Surface reflectance
*/

printf("===STAGE 1: DN -> TOA Radiance -> Surface reflectance===\n");
    for(band=3;band>=1;band--){
      printf("Reading data from band %d...",band);
      bandArray[0] = 1;
      Mat imageBuffer;
      imageBuffer.create(ysize, xsize, CV_16UC1);
      // Load data in as uint16_t (i.e., GDT_UInt16 or CV_16U)
      imageBuffer.zeros(ysize, xsize, CV_16UC1);
      poDataset[band-1]->RasterIO(GF_Read, 0, 0, xsize, ysize,
                                         (void*) imageBuffer.ptr(0), xsize, ysize,
                                         GDT_UInt16, 1, bandArray, 0, 0, 0);
  // Find the minimum non-zero value which would be corresponds to Lhaze_1%rad later
      uint16_t minNonZero = 65535;
      uint64_t noDataCount = 0;
      #pragma omp parallel for reduction(min:minNonZero) reduction(+:noDataCount)
      for(int x=0;x<xsize;x++){
        for(int y=0;y<ysize;y++){
          uint16_t value = ((uint16_t*)(imageBuffer.data))[y*xsize+x];
          if(value==0){noDataCount++;}
          else if(value<minNonZero){minNonZero=value;}
        }
      }

  /* Get the TOA reflectance, solar angle adjusted with metadata basename_MTL.txt
     For landsat 7 documentation is at: http://landsathandbook.gsfc.nasa.gov/data_prod/prog_sect11_3.html
                                        http://www.yale.edu/ceo/Documentation/Landsat_DN_to_Reflectance.pdf
     For landsat 8 documentation is at: http://www.gisagmaps.com/landsat-8-data-tutorial/
  */

      Mat noData = (imageBuffer==0);
// Conversion to TOA Radiance
      sprintf(itemname, "RADIANCE_MULT_BAND_%u = ", band);
      multValue = readMeta(itemname);
      sprintf(itemname, "RADIANCE_ADD_BAND_%u = ", band);
      addValue = readMeta(itemname);
      // Pixels Being zero in DN is always regarded as no data
      imageBuffer.convertTo(imageBuffer, CV_32F, multValue, addValue);

// Conversion to Surface Reflectance using DOS (Dark-object Subtraction)
// [Chavez P.S. 1996], TAUv and TAUz both equals 1.0, Edown = 0.0

      char doy_string[4];
      memcpy(doy_string,basename+13,3);doy_string[3]='\0';
      fprintf(stderr,"EARTH_SUN_DISTANCE computed from DOY=%d!", atoi(doy_string));
      float distance = 1.0-0.01674*cos(0.9856*(atoi(doy_string)-4)*PI/180);
      // ESUN should be working but a bluish tint is still there. Investigating.
      // Bands without values are, at the time being, assigned to be 1000.0
      // Will have to compute ESUN values on my own later
      float Esun[9] = {1969.0, 1840.0, 1551.0, 1044.0, 255.7, 1000.0, 1000.0, 82.07, 1000.0};
      float angle = 90.0-readMeta("SUN_ELEVATION = ");
      // A loop to find the Lhaze_1%rad.
      float lHazeOnePercent = minNonZero*multValue+addValue;
      imageBuffer = PI*(imageBuffer-lHazeOnePercent)*distance*distance/(Esun[band-1]*cos(PI*angle/180.0));

// Re-enact no-data mask
      imageBuffer = imageBuffer.setTo(NAN, noData);

// Copy to output array
      float min = 1e9;
      float max = -1e9;
      float* imgData = ((float*)(imageBuffer.data));
      float* outData = ((float*)(outputBuffer.data));
      #pragma omp parallel for reduction(min:min) reduction(max:max)
      for(int x=0;x<xsize;x++){
        for(int y=0;y<ysize;y++){
          if(imgData[y*xsize+x] < min){min = imgData[y*xsize+x];}
          if(imgData[y*xsize+x] > max){max = imgData[y*xsize+x];}
          outData[(y*xsize+x)*3+(band-1)] = imgData[y*xsize+x];
        }
      }
      printf("...done (noData pixels = %lu, Min nonzero value is %u, minmax = %f, %f)\n", noDataCount, minNonZero, min, max);
    }

/* Image masking and spatial processing: Pipeline as follows:
   Part(2): Processing with all bands
            Cloud Masking -> (Cloud-shadow Masking) -> Maskout no data area -> De-haze
   At the time I do not plan on cloud-shadow masking yet...
*/
printf("===STAGE 2: Mask Creation===\n");
// Cloud mask does not exists, so only data mask is done

   /*Block for the masking -- abusing C++ scoping*/{
      uint64_t noDataCount = 0;
      #pragma omp parallel for reduction(+:noDataCount) collapse(2)
      for(int x=0;x<xsize;x++){
        for(int y=0;y<ysize;y++){
          float* outBfrData = (float*)(outputBuffer.data);
          float value0 = outBfrData[(y*xsize+x)*3+0];
          float value1 = outBfrData[(y*xsize+x)*3+1];
          float value2 = outBfrData[(y*xsize+x)*3+2];
          if(isnan(value0) || isnan(value1) || isnan(value2)){
            noDataCount++;
            outBfrData[(y*xsize+x)*3+0] = outBfrData[(y*xsize+x)*3+1] = outBfrData[(y*xsize+x)*3+2] = NAN;
          }
        }
      }
     printf("Mask created. %u/%u pixels masked out.\n", noDataCount, xsize*ysize);
  }

//// SPECIAL COMMENT ON 13 Apr 2014: Since the upcoming research will involve searching for replacement of PANSHARP, so it is not implemented here
/* Image masking and spatial processing: Pipeline as follows:
   Part(3): Zooming, Read Panochromatic -> PANSHARP
            -> Output 3-channel PANSHARP-ed+Alpha 32-bit GeoTIFF, with NaN if necessary (OpenCV imwrite is not GeoTIFF/32-bit depth friendly)
*/
printf("===STAGE 3: Pansharp=== (SKIPPED)\n");
  // One should be able to get coordinate with GDALApplyGeoTransform() (or maybe also GDALInvGeoTransform())

printf("===STAGE 4: Store the Result===\n");
  normalize(outputBuffer, outputBuffer, 65536, 0, NORM_MINMAX, -1);
  //outputBuffer.convertTo(outputBuffer, CV_16U);

  // Saving using GDAL's writing facility
    // Create driver and dataset
    GDALDataset *outputDataset = NULL;
    GDALDriver* geoTiffDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    char **papszMetadata;

    if(geoTiffDriver == NULL){
      fprintf(stderr,"Cannot retrieve Driver!\n");exit(-1);
    }

    papszMetadata = GDALGetMetadata(geoTiffDriver, NULL);
    if(!CSLFetchBoolean(papszMetadata, GDAL_DCAP_CREATE, FALSE)){
      fprintf(stderr,"Driver %s does not support Create() method.\n", "GTiff");
    }
    char** papszOptions = NULL;
    papszOptions = CSLSetNameValue( papszOptions, "PHOTOMETRIC", "RGB" );

    // OpenCV -> Pixel interleaving; GDAL -> (Should I choose?)
    // (The suffix must be .tif? Strange GDAL)
    sprintf(filename,"%s_clear.tif",basename);
    outputDataset = geoTiffDriver->Create(filename, xsize, ysize, 3, GDT_UInt16, papszOptions);
    if(outputDataset == NULL){
      fprintf(stderr,"Cannot open new dataset.\n");exit(-1);
    }
    CSLDestroy(papszOptions);

    // Copy projection and georef from the band 1 source GeoTIFF
    double affine[6];
    if(GDALGetGeoTransform(poDataset[0], affine) == CE_None) {
      GDALSetGeoTransform(outputDataset, affine);
    }
    GDALSetProjection(outputDataset, GDALGetProjectionRef(poDataset[0]));
    fprintf(stderr,"Georeference Set!\n");

    // Copy data from the OpenCV image to GDAL
    Mat imageBuffer;
    imageBuffer.create(ysize, xsize, CV_32F);
    float* imgData = ((float*)(imageBuffer.data));
    float* outData = ((float*)(outputBuffer.data));
    for(band=3;band>=1;band--){
      bandArray[0]=4-band;
      printf("Writing data to band %d\n", band);
      #pragma omp parallel for collapse(2)
      for(int x=0;x<xsize;x++){
        for(int y=0;y<ysize;y++){
          imgData[y*xsize+x] = outData[(y*xsize+x)*3+(band-1)];
        }
      }
      outputDataset->RasterIO(GF_Write, 0, 0, xsize, ysize,
                                         (void*) imageBuffer.ptr(0), xsize, ysize,
                                         GDT_Float32, 1, bandArray, 0, 0, 0);
    }

    // Set the Alpha channel?
    fprintf(stderr,"ALPHA channel not set yet\n");

    delete outputDataset; // Close file?

  // Free the resources
    for(band=0;band<=8;band++){
      delete poDataset[band];
      poDataset[band] = NULL;
    }
    free(filename);

  printf("Program completed normally.\n");
  return 0;
}
