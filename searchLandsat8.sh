#!/bin/bash

wget -q -N http://landsat.usgs.gov/metadata_service/bulk_metadata_files/LANDSAT_8.csv

cut -d "," -f 1-20,25,27,28,29,30,31,33,34,54,55 LANDSAT_8.csv > landsat8.csv

cat > landsat.db </dev/null

sqlite3 landsat.db << END
CREATE TABLE landsat8 (
  sceneID string,sensor string,
  acquisitionDate date,dateUpdated date,
  browseAvailable char,browseURL string,
  path integer,row integer,
  ULLat float,ULLon float,URLat float,URLon float,
  LLLat float,LLLon float,LRLat float,LRLon float,
  centerLat float,centerLon float,
  cloudCover integer,cloudCoverFull numeric(6,3),
  dayOrNight char,sunElev float,sunAzi float,
  recvStation string,
  sceneStartTime datetime,sceneStopTime datetime,
  imageQ1 integer,imageQ2 integer,
  dataType string,
  cartURL string
);
.separator ,
.import landsat8.csv landsat8
END

sqlite3 landsat.db << END
.output stdout
select sceneID, acquisitionDate, cloudCoverFull, sunElev, sunAzi, imageQ1, dataType, cartURL from landsat8 where path
=$1 and row=$2 and dayorNight='DAY' order by acquisitionDate;
END

#http://earthexplorer.usgs.gov/order/process?dataset_name=LANDSAT_8&ordered=LC81210452014073LGN00
#https://earthexplorer.usgs.gov/download/options/4923/LC81210452014073LGN00?node=EE
#http://earthexplorer.usgs.gov/download/4923/LC81210452014073LGN00/STANDARD/EE
