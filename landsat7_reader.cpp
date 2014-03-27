/*
Notice that this code is just a branch of the unfinished landsat8 program on 27Mar2014
We fought for common processing pipeline but is severely affected by the memory bound
That's why we are splitting the effort.

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
  uint16_t numChannel = 8;  // Hard-coded as we know there must be 11 channels
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
    outputBuffer.create(ysize, xsize, CV_32FC(numChannel));
    outputBuffer.zeros(ysize, xsize, CV_32FC(numChannel));

/* Image reading and adjust: Pipeline as follows:
   Part(1): Band-by-band Possible Processing
            DN -> TOA Radiance -> Surface reflectance
*/
    for(band_number=1;band_number<=numChannel;band_number++){
      if(band_number!=8){
        printf("Reading data from band %d...",band_number);
        bandArray[0] = 1;
        Mat imageBuffer;
        imageBuffer.create(ysize, xsize, CV_16UC1);
  // Load data in as uint16_t (i.e., GDT_UInt16 or CV_16U)
        imageBuffer.zeros(ysize, xsize, CV_16UC1);
        poDataset[band_number-1]->RasterIO(GF_Read, 0, 0, xsize, ysize,
                                           (void*) imageBuffer.ptr(0), xsize, ysize,
                                           GDT_UInt16, 1, bandArray, 0, 0, 0);
    // Find the minimum non-zero value which would be corresponds to Lhaze_1%rad later
        uint16_t minNonZero = 65535;
        uint64_t noDataCount = 0;
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
        sprintf(itemname, "RADIANCE_MULT_BAND_%u = ", band_number);
        multValue = readMeta(itemname);
        sprintf(itemname, "RADIANCE_ADD_BAND_%u = ", band_number);
        addValue = readMeta(itemname);
        // Pixels Being zero in DN is always regarded as no data
        imageBuffer.convertTo(imageBuffer, CV_32F, multValue, addValue);

  // Conversion to Surface Reflectance using DOS (Dark-object Subtraction)
  // [Chavez P.S. 1996], TAUv and TAUz both equals 1.0, Edown = 0.0

        float distance = readMeta("EARTH_SUN_DISTANCE = ");
    // ESUN values NOT from USGS/NASA, but here http://www.gisagmaps.com/landsat-8-atco/
    // Bands without values are, at the time being, assigned to be 1000.0
    // Will have to compute ESUN values on my own later
        float Esun[11] = {1000.0, 2067.0, 1893.0, 1603.0, 972.6, 245.0, 79.72, 1000.0, 399.7, 1000.0, 1000.0};
        float angle = 90.0-readMeta("SUN_ELEVATION = ");
    // A loop to find the Lhaze_1%rad.
        float lHazeOnePercent = minNonZero*multValue+addValue;
        imageBuffer = PI*(imageBuffer-lHazeOnePercent)*distance*distance/(Esun[band_number-1]*cos(PI*angle/180.0));

  // Re-enact no-data mask
        //imageBuffer = imageBuffer.setTo(NAN, noData);

  // Copy to output array
        float min = 1e9;
        float max = -1e9;
        for(int x=0;x<xsize;x++){
          for(int y=0;y<ysize;y++){
            if(((float*)(imageBuffer.data))[y*xsize+x] < min){min =((float*)(imageBuffer.data))[y*xsize+x];}
            if(((float*)(imageBuffer.data))[y*xsize+x] > max){max =((float*)(imageBuffer.data))[y*xsize+x];}
            ((float*)(outputBuffer.data))[(y*xsize+x)*numChannel+(band_number-1)] = ((float*)(imageBuffer.data))[y*xsize+x];
          }
        }
        printf("...done (noData pixels = %lu, Min nonzero value is %u, minmax = %f, %f)\n", noDataCount, minNonZero, min, max);
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

  // Temporary saving routine converting the data to 8-bit RGB JPEG
  Mat outputPNG( ysize, xsize, CV_32FC3);
  normalize(outputBuffer, outputBuffer, 65536, 0, NORM_MINMAX, -1);
/*
  printf("Normalize!!\n");
  for(band_number=1;band_number<=numChannel;band_number++){
    float min = 1e9;
    float max = -1e9;
    for(int x=0;x<xsize;x++){
      for(int y=0;y<ysize;y++){
        float value = ((float*)(outputBuffer.data))[(y*xsize+x)*numChannel+(band_number-1)];
        if(value < min){min = value;}
        if(value > max){max = value;}
      }
    }
    printf("Channel %u, min = %f\tmax = %f\n", band_number, min, max);
  }
*/

  // forming an array of matrices is a quite efficient operation,
  // because the matrix data is not copied, only the headers
  int from_to[] = {2-1,0, 3-1,1, 4-1,2};
  mixChannels( &outputBuffer, 1, &outputPNG, 1, from_to, 3);
  outputBuffer.create(1, 1, CV_8U); // Indirectly delete all things in outputBuffer
  outputPNG.convertTo(outputPNG, CV_16U);
  //resize(outputPNG, outputPNG, Size(), 0.1, 0.1, INTER_NEAREST);
  imwrite("test.tiff", outputPNG);

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

  printf("Program completed normally.\n");
  return 0;
}
