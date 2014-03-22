#!/bin/bash

make modis_test
for j in Aqua;do
  for i in {1..365};do
    ./modis_test $j `date -d"01Jan2013 -1 day + "$i" days" +"%Y %m %d"`
  done
done
