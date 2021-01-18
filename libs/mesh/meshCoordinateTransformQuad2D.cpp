/*

The MIT License (MIT)

Copyright (c) 2017 Tim Warburton, Noel Chalmers, Jesse Chan, Ali Karakus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "mesh.hpp"
#include "mesh/mesh2D.hpp"

void meshQuad2D::CoordinateTransform(int _cubN, const char *_cubatureType){

  cubatureType = strdup(_cubatureType);
  
  printf("CoordinateTransform\n");
  
  /* */
  string mapFileName;
  settings.getSetting("BOX COORDINATE MAP FILE", mapFileName);

  if(1)
  if(mapFileName != "NONE"){
    printf("MAPPING COORDINATES\n");
    
    dfloat epsy = 1.;
    settings.getSetting("BOX COORDINATE MAP PARAMETER Y", epsy);
    occa::properties kernelInfo = props;

    int coordMapModel = 1;
    settings.getSetting("BOX COORDINATE MAP MODEL", coordMapModel);
    props["defines/p_mapModel"] = coordMapModel;
    
    // build kernel
    occa::kernel coordMapKernel = platform.buildKernel(mapFileName, "coordMapKernel", kernelInfo);
    
    occa::memory o_tmpx, o_tmpy, o_tmpz, o_tmpEX, o_tmpEY, o_tmpEZ;
    o_tmpx = platform.device.malloc(Np*Nelements*sizeof(dfloat), x);
    o_tmpy = platform.device.malloc(Np*Nelements*sizeof(dfloat), y);
    
    coordMapKernel(Np*Nelements, epsy, o_tmpx, o_tmpy);

    o_tmpEX = platform.device.malloc(Nverts*Nelements*sizeof(dfloat), EX);
    o_tmpEY = platform.device.malloc(Nverts*Nelements*sizeof(dfloat), EY);

    coordMapKernel(Nverts*Nelements, epsy, o_tmpEX, o_tmpEY);

    o_tmpx.copyTo(x);
    o_tmpy.copyTo(y);

    o_tmpEX.copyTo(EX);
    o_tmpEY.copyTo(EY);

  }
  
  halo->Exchange(x, Np, ogs_dfloat);
  halo->Exchange(y, Np, ogs_dfloat);

  // compute geometric factors
  GeometricFactors();

  // compute surface geofacs
  SurfaceGeometricFactors();

  // compute cubature stuff
  CubatureSetup(_cubN, _cubatureType);
  
  // copy to DEVICE
  o_vgeo = platform.malloc((Nelements+totalHaloPairs)*Nvgeo*Np*sizeof(dfloat), vgeo);
  o_sgeo = platform.malloc(Nelements*Nfaces*Nfp*Nsgeo*sizeof(dfloat), sgeo);
  o_ggeo = platform.malloc(Nelements*Np*Nggeo*sizeof(dfloat), ggeo);

}