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

#include <mpi.h>
#include "mesh3D.h"
#include "ellipticTet3D.h"

// 1 Advection Volume 2 Advection Surface 3 Ax 4 Gradient
#define KERNEL_TEST 1

void insRunTimerTet3D(mesh3D *mesh, char *options, char *boundaryHeaderFileName){

  char deviceConfig[BUFSIZ];
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  mesh->Nfields = 1;

  
  // use rank to choose DEVICE
  sprintf(deviceConfig, "mode = CUDA, deviceID = %d", (rank)%3);

  occa::kernelInfo kernelInfo;
  meshOccaSetup3D(mesh, deviceConfig, kernelInfo);
  
  kernelInfo.addInclude(boundaryHeaderFileName);


  int index = 0, iterations = 1000,  Nbytes=0,  zero = 0;  
  dfloat lambda = 0.0; 
  dfloat time = 0.0; 
  int  Ntotal    = (mesh->Nelements+mesh->totalHaloPairs)*mesh->Np;
  int  cubNtotal = (mesh->Nelements+mesh->totalHaloPairs)*mesh->cubNp;

  dfloat *Z     = (dfloat*) calloc(Ntotal,sizeof(dfloat));
  dfloat *cZ    = (dfloat*) calloc(cubNtotal,sizeof(dfloat));

  dfloat *G     = (dfloat*) calloc(Ntotal*4, sizeof(dfloat));
  

  occa::memory o_U, o_V, o_W, o_X, o_Y, o_Z, o_Ud, o_Vd, o_Wd, o_G;
  occa::memory o_cU, o_cV, o_cW,  o_cUd, o_cVd, o_cWd; 
  o_U   = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_V   = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_W   = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_Ud  = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_Vd  = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_Wd  = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);

  o_cU   = mesh->device.malloc(cubNtotal*sizeof(dfloat),cZ);
  o_cV   = mesh->device.malloc(cubNtotal*sizeof(dfloat),cZ);
  o_cW   = mesh->device.malloc(cubNtotal*sizeof(dfloat),cZ);
  o_cUd  = mesh->device.malloc(cubNtotal*sizeof(dfloat),cZ);
  o_cVd  = mesh->device.malloc(cubNtotal*sizeof(dfloat),cZ);
  o_cWd  = mesh->device.malloc(cubNtotal*sizeof(dfloat),cZ);


  o_X   = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_Y   = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_Z   = mesh->device.malloc(Ntotal*sizeof(dfloat),Z);
  o_G   = mesh->device.malloc(Ntotal*4*sizeof(dfloat),G); 

  free(Z); free(G); free(cZ);

  printf("Nelements: %d\n", mesh->Nelements);
  int maxNodes = mymax(mesh->Np, (mesh->Nfp*mesh->Nfaces));
  kernelInfo.addDefine("p_maxNodes", maxNodes);

  int NblockV = 256/mesh->Np; // works for CUDA
  kernelInfo.addDefine("p_NblockV", NblockV);

  int NblockS = 256/maxNodes; // works for CUDA
  kernelInfo.addDefine("p_NblockS", NblockS);
  

  int maxNodesVolumeCub = mymax(mesh->cubNp,mesh->Np);  
  kernelInfo.addDefine("p_maxNodesVolumeCub", maxNodesVolumeCub);
  int cubNblockV = 256/maxNodesVolumeCub; 
  //
  int maxNodesSurfaceCub = mymax(mesh->Np, mymax(mesh->Nfaces*mesh->Nfp, mesh->Nfaces*mesh->intNfp));
  kernelInfo.addDefine("p_maxNodesSurfaceCub",maxNodesSurfaceCub);
  int cubNblockS = 256/maxNodesSurfaceCub; // works for CUDA
  //
  kernelInfo.addDefine("p_cubNblockV",cubNblockV);
  kernelInfo.addDefine("p_cubNblockS",cubNblockS);
  
  double flops;
 
  int Np      = mesh->Np;
  int Nc      = mesh->cubNp;
  int Nfp     = mesh->Nfp; 
  int Ntfp    = mesh->Nfaces*mesh->Nfp; 
  int intNtfp = mesh->Nfaces*mesh->intNfp;
  double tic = 0.0, toc = 0.0, kernelElapsed=0.0;
  int NbytesShared = 0; 


  occa::kernel TestKernel; 

  #if KERNEL_TEST==1
  int NKernels = 6;

  occa::kernel *testKernels = new occa::kernel[NKernels];
  char kernelNames[NKernels][BUFSIZ];

  for(int i=0; i<NKernels; i++)
  {
    
    sprintf(kernelNames[i], "insSubCycleCubatureVolume3D_v%d", i);

    testKernels[i] = mesh->device.buildKernelFromSource(DHOLMES "/okl/insSubCycle3D.okl",kernelNames[i], kernelInfo);

    printf("Nblock: %d cubNblock: %d N: %d Np: %d cubNp: %d\n", NblockV, cubNblockV, mesh->N, mesh->Np, mesh->cubNp);


    // sync processes
    mesh->device.finish();
    MPI_Barrier(MPI_COMM_WORLD);

    //occaTimerTic(mesh->device,"KernelTime");
    tic = MPI_Wtime();  
      // assume 1 mpi process
      for(int it=0;it<iterations;++it){
          testKernels[i](mesh->Nelements,
                  mesh->o_vgeo,
                  mesh->o_cubDrWT,
                  mesh->o_cubDsWT,
                  mesh->o_cubDtWT,
                  mesh->o_cubInterpT,
                  o_U,
                  o_V,
                  o_W,
                  o_Ud,
                  o_Vd,
                  o_Wd,
                  o_X,
                  o_Y,
                  o_Z);

      }

      occa::streamTag end = mesh->device.tagStream();
      mesh->device.finish();  
      toc = MPI_Wtime();
      kernelElapsed    = toc-tic;

       if(i==0){
        Nbytes =(sizeof(dfloat)*(6*Np*Nc + 9*Np + 3*Np)/2);
        flops = Nc*Np*12 + Nc*9 + Np*Nc*54 + Np*42 + Np*3;  // All float ops only
        NbytesShared     = (sizeof(dfloat)*(9*Nc + 9*Np*Nc)); 
      }
      else
      {
        Nbytes =(sizeof(dfloat)*(9*mesh->Np +9*mesh->Np)/2);
        flops = Nc*Np*12 + Nc*9 + Np*Nc*54 + Np*42 + Np*3;  // All float ops only
        NbytesShared     = (sizeof(dfloat)*(6*Np + 6*Np*Nc + 9*Nc + 9*Np*Nc)); 
      }
     
     
      
      
      occa::memory o_foo = mesh->device.malloc(Nbytes*mesh->Nelements);
      occa::memory o_bah = mesh->device.malloc(Nbytes*mesh->Nelements);

      mesh->device.finish(); 
      tic = MPI_Wtime();

      //occa::streamTag startCopy = mesh->device.tagStream();
      for(int it=0;it<iterations;++it){
         o_bah.copyTo(o_foo);
      }
      //occa::streamTag endCopy = mesh->device.tagStream();

      mesh->device.finish();
      toc = MPI_Wtime();
      double copyElapsed = (toc-tic);
      //copyElapsed = mesh->device.timeBetween(startCopy, endCopy);

 // Compute Data
      double copyBandwidth = mesh->Nelements*((Nbytes*iterations*2)/(1024.*1024.*1024.*copyElapsed));
      double  bw           = mesh->Nelements*((Nbytes*iterations*2)/(1024.*1024.*1024.*kernelElapsed));


      double gflops        = mesh->Nelements*flops*iterations/(1024*1024*1024.*kernelElapsed);
      double d2dbound      = copyBandwidth*gflops/bw;

     

      double smbound       = 7340.5*flops/( (double) NbytesShared);

      printf("SMBOUND: %17.15e\n",smbound);

  
      double intensity     = gflops/bw; 

      double roofline      = mymin(d2dbound, smbound);

      double max_thg_p100 = 4670; 
      double ach_thg      = mymin(549*intensity, max_thg_p100);



      printf("[ N\tK\tDOFS\tKernelTime\tCopyTime\tIntensity\tGFLOPS/s\t(d2dbound GFLOPS/s)\tBW(GB/s)\t(SMBOUND GFLOPS/s)\t(ROOFLINE GFLOPS/s)\tTH_peak]\n");
      printf("%02d %02d\t%02d\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\n",
              mesh->N, mesh->Nelements,(mesh->Nelements*mesh->Np), 
              kernelElapsed, copyElapsed, intensity, gflops, d2dbound, bw, smbound, roofline, ach_thg);

      char fname[BUFSIZ];
      sprintf(fname, "KernelData.txt");
      FILE *fp;
      fp = fopen(fname, "a");
      fprintf(fp,"%02d %02d\t%02d\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\t%12.10E\n",
              mesh->N, mesh->Nelements,(mesh->Nelements*mesh->Np), 
              kernelElapsed, copyElapsed, intensity, gflops, d2dbound, bw, smbound, roofline, ach_thg);
      fclose(fp);

    }

  #endif



  




 
  #if 0

   #if KERNEL_TEST==2

  TestKernel = mesh->device.buildKernelFromSource(DHOLMES 
                            "/okl/insSubCycle2D.okl","insSubCycleCubatureSurface2D"
                            ,kernelInfo);
  int KernelId = 0; // Cubature Integration


  // TestKernel = mesh->device.buildKernelFromSource(DHOLMES 
  //                           "/okl/insAdvection2D.okl","insAdvectionSurface2D"
  //                           ,kernelInfo);
  // int KernelId = 1; // Cubature Integration

  printf("Nblock: %d cubNblock: %d N: %d Nfp: %d cubNfp: %d\n", NblockS, cubNblockS, mesh->N, mesh->Nfp, mesh->intNfp);
  
  #endif


 
 
 double memory_used = mesh->device.memoryAllocated()/(1024.*1024.*1024);
 printf("Memory Allocated (GB) = %g\n", memory_used);


 
  

  double tic = 0.0, toc = 0.0, kernelElapsed=0.0;

 if(KernelId==0){
    // sync processes
    mesh->device.finish();
    MPI_Barrier(MPI_COMM_WORLD);

    //occaTimerTic(mesh->device,"KernelTime");
    tic = MPI_Wtime();  
    occa::streamTag start = mesh->device.tagStream();

    // assume 1 mpi process
    for(int it=0;it<iterations;++it){
      #if KERNEL_TEST==1
      //printf("Cubature Points: %d", mesh->cubNp);
      TestKernel(mesh->Nelements,
                mesh->o_vgeo,
                mesh->o_cubDrWT,
                mesh->o_cubDsWT,
                mesh->o_cubInterpT,
                o_U,
                o_V,
                o_Ud,
                o_Vd,
                o_X,
                o_Y);
      #endif

      #if KERNEL_TEST==2
       TestKernel(mesh->Nelements,
                mesh->o_sgeo,
                mesh->o_intInterpT,
                mesh->o_intLIFTT,
                mesh->o_vmapM,
                mesh->o_vmapP,
                mesh->o_EToB,
                time,
                mesh->o_intx,
                mesh->o_inty,
                o_U,
                o_V,
                o_Ud,
                o_Vd,
                o_X,
                o_Y);
        #endif

    }

    occa::streamTag end = mesh->device.tagStream();
    mesh->device.finish();  
    toc = MPI_Wtime();
    kernelElapsed    = toc-tic;
    kernelElapsed    = mesh->device.timeBetween(start, end);
}
if(KernelId==1){
    // sync processes
    mesh->device.finish();
    MPI_Barrier(MPI_COMM_WORLD);

    //occaTimerTic(mesh->device,"KernelTime");
    tic = MPI_Wtime();  
    occa::streamTag start = mesh->device.tagStream();

    // assume 1 mpi process
    for(int it=0;it<iterations;++it){
      #if KERNEL_TEST==1
        TestKernel(mesh->Nelements,
                  mesh->o_vgeo,
                  mesh->o_DrT,
                  mesh->o_DsT,
                  zero,
                  o_U,
                  o_V,
                  o_X,
                  o_Y);

      #endif


      #if KERNEL_TEST==2
       TestKernel(mesh->Nelements,
        mesh->o_sgeo,
        mesh->o_LIFTT,
        mesh->o_vmapM,
        mesh->o_vmapP,
        mesh->o_EToB,
        time,
        mesh->o_x,
        mesh->o_y,
        index,
        o_U,
        o_V,
        o_X,
        o_Y);
      #endif

    }

    occa::streamTag end = mesh->device.tagStream();
    mesh->device.finish();  
    toc = MPI_Wtime();
    kernelElapsed    = toc-tic;
}

  
 
  

  #if KERNEL_TEST==1
    if(KernelId==0){ // Cubature Integration
     flops = Nc*Np*8 + 4*Nc + Np*Nc*16 + Np*14 + Np*2;  // All float ops only
     Nbytes =(sizeof(dfloat)*(6*mesh->Np +4*mesh->Np)/2);

    }
    else{
    flops =  3*Np + Np*Np*12 + Np*14;
    Nbytes =(sizeof(dfloat)*(2*mesh->Np  + 4*mesh->Np+ 2*mesh->Np)/2);

    }
  #endif

  #if KERNEL_TEST==2
    if(KernelId==0){ 
     flops = intNtfp*( Nfp*16 + 6 + 1 + 28) + Np*intNtfp*4;  // All float ops only  
      //Nbytes = (sizeof(dfloat)*(8*Ntfp + 4*intNtfp + 4*mesh->Np))/2;  
      Nbytes = (sizeof(dfloat)*(8*Ntfp + 4*mesh->Nfaces + 4*mesh->Np))/2;  //?????  
     
    }

    if(KernelId==1){ 
     flops = Ntfp*(6 + 28 + 1) + Np*Ntfp*4;  // All float ops only  
      Nbytes = (sizeof(dfloat)*(8*Ntfp + 4*mesh->Np))/2;   
     
    }

  #endif
  
   #if KERNEL_TEST==3
   flops = 1*Np + 21*Ntfp + Np*(4*Ntfp + 8) +6*Ntfp+ Np*(4*Np + 2)+ Np*(1+2*Ntfp) + Np*(1+2*Np);  // All float ops only      
  #endif

  #if KERNEL_TEST==4
   flops = Np*(Np*2 + Np*2 + 6);  // All float ops only      
  
  #endif






  occa::memory o_foo = mesh->device.malloc(Nbytes*mesh->Nelements);
  occa::memory o_bah = mesh->device.malloc(Nbytes*mesh->Nelements);
  
  mesh->device.finish(); 
  tic = MPI_Wtime();

  occa::streamTag startCopy = mesh->device.tagStream();
  for(int it=0;it<iterations;++it){
    o_bah.copyTo(o_foo);
  }
  occa::streamTag endCopy = mesh->device.tagStream();
  
  mesh->device.finish();
  toc = MPI_Wtime();
  double copyElapsed = (toc-tic);
  copyElapsed = mesh->device.timeBetween(startCopy, endCopy);
   
  
  // Compute Data
 double copyBandwidth = mesh->Nelements*((Nbytes*iterations*2)/(1024.*1024.*1024.*copyElapsed));
 double  bw           = mesh->Nelements*((Nbytes*iterations*2)/(1024.*1024.*1024.*kernelElapsed));

  
  double gflops   = mesh->Nelements*flops*iterations/(1024*1024*1024.*kernelElapsed);
  double d2dbound = copyBandwidth*gflops/bw;


  // Intensity = flops/bytes
  double intensity = gflops/bw; 

  double max_thg_p100 = 4700; 
  double ach_thg     = mymin(549*intensity, max_thg_p100);

  printf("[ N\tK\tDOFS\tKernelTime\tCopyTime\tIntensity\tGFLOPS/s\t(d2dbound GFLOPS/s)\tBW(GB/s)\tTH_peak\t(GFLOPS/s)]\n");
  printf("%02d %02d\t%02d\t%17.15E\t%17.15E\t%17.15E\t%17.15E\t%17.15E\t%17.15E\t%17.15E\n",
              mesh->N, mesh->Nelements,(mesh->Nelements*mesh->Np), 
              kernelElapsed, copyElapsed, intensity, gflops, d2dbound, bw, ach_thg);

    char fname[BUFSIZ];
    sprintf(fname, "KernelData.dat");
    FILE *fp;
    fp = fopen(fname, "a");

    fprintf(fp,"%02d %02d\t%02d\t%17.15E\t%17.15E\t%17.15E\t%17.15E\t%17.15E\t%17.15E\t%17.15E\n",
              mesh->N, mesh->Nelements,(mesh->Nelements*mesh->Np), 
              kernelElapsed, copyElapsed, intensity, gflops, d2dbound, bw, ach_thg);
    fclose(fp);
  // }

#endif
  




#if 0
  dfloat time = 0.0; 
  int iterations = 10;
  // sync processes
  mesh->device.finish();
  MPI_Barrier(MPI_COMM_WORLD);

 //occaTimerTic(mesh->device,"KernelTime");
  double tic = MPI_Wtime();  
  occa::streamTag start = mesh->device.tagStream();

  // assume 1 mpi process
  for(int it=0;it<iterations;++it){
  
  #if KERNEL_TEST==1
    //printf("Cubature Points: %d", mesh->cubNp);
    testKernel1(mesh->Nelements,
                mesh->o_vgeo,
                mesh->o_cubDrWT,
                mesh->o_cubDsWT,
                mesh->o_cubInterpT,
                o_U,
                o_V,
                o_Ud,
                o_Vd,
                o_X,
                o_Y);
  #endif

  #if KERNEL_TEST==2
  ins->subCycleCubatureSurfaceKernel(mesh->Nelements,
              mesh->o_sgeo,
              mesh->o_intInterpT,
              mesh->o_intLIFTT,
              mesh->o_vmapM,
              mesh->o_vmapP,
              mesh->o_EToB,
              time,
              mesh->o_intx,
              mesh->o_inty,
              o_U,
              o_V,
              o_Ud,
              o_Vd,
              o_X,
              o_Y);
  #endif

  #if KERNEL_TEST==3
  solver->partialIpdgKernel(mesh->NinternalElements,
                                  mesh->o_internalElementIds,
                                  mesh->o_vmapM,
                                  mesh->o_vmapP,
                                  lambda,
                                  solver->tau,
                                  mesh->o_vgeo,
                                  mesh->o_sgeo,
                                  solver->o_EToB,
                                  mesh->o_DrT,
                                  mesh->o_DsT,
                                  mesh->o_LIFTT,
                                  mesh->o_MM,
                                  o_G,
                                  o_X);
  #endif

  #if KERNEL_TEST==4
  solver->partialGradientKernel(mesh->Nelements,
                                  ins->index, //zero
                                  mesh->o_vgeo,
                                  mesh->o_DrT,
                                  mesh->o_DsT,
                                  o_U,
                                  o_G);
  #endif


}
  occa::streamTag end = mesh->device.tagStream();
  mesh->device.finish();  
  double toc = MPI_Wtime();
  double localElapsed    = toc-tic;
  
  // Compute memory copy of the kernel
  int Nbytes;
  #if KERNEL_TEST==1
  Nbytes =(sizeof(dfloat)*(6*mesh->Np +4*mesh->Np)/2);
  #endif

  #if KERNEL_TEST==2
  Nbytes = (sizeof(dfloat)*(8*mesh->Nfaces*mesh->Nfp 
                           +4*mesh->Np                   //Field variables
                           +4*mesh->Nfaces // Geometric factors - BCType
                           ))/2;
  #endif

  #if KERNEL_TEST==3
  Nbytes = (sizeof(dfloat)*(4*mesh->Np    // float4
                            +8*mesh->Nfaces*mesh->Nfp // float4
                            +5*mesh->Nfaces // cached -> *mesh->Nfp
                            +6*mesh->Np))/2;
  #endif

  #if KERNEL_TEST==4
  Nbytes = (sizeof(dfloat)*(5*mesh->Np    // float4
                            +4*mesh->Np))/2;
  #endif

  occa::memory o_foo = mesh->device.malloc(Nbytes*mesh->Nelements);
  occa::memory o_bah = mesh->device.malloc(Nbytes*mesh->Nelements);
  
  mesh->device.finish(); 
  tic = MPI_Wtime();

  occa::streamTag startCopy = mesh->device.tagStream();
  for(int it=0;it<iterations;++it){
    o_bah.copyTo(o_foo);
  }
  occa::streamTag endCopy = mesh->device.tagStream();
  
  mesh->device.finish();
  toc = MPI_Wtime();
  double copyElapsed = (toc-tic);
  copyElapsed = mesh->device.timeBetween(startCopy, endCopy);
  
  
  double copyBandwidth = mesh->Nelements*((Nbytes*iterations*2)/(1024.*1024.*1024.*copyElapsed));

  int   localDofs = mesh->Np*mesh->Nelements;
  int   localElements = mesh->Nelements;
  double globalElapsed;
  int   globalDofs;
  int   globalElements;
  int    root = 0;
  
  MPI_Reduce(&localElapsed, &globalElapsed, 1, MPI_DOUBLE, MPI_MAX, root, MPI_COMM_WORLD );
  MPI_Reduce(&localDofs,    &globalDofs,    1, MPI_INT,   MPI_SUM, root, MPI_COMM_WORLD );
  MPI_Reduce(&localElements,&globalElements,1, MPI_INT,   MPI_SUM, root, MPI_COMM_WORLD );
  
  int Np      = mesh->Np;
  int Nc      = mesh->cubNp;
  int Nfp     = mesh->Nfaces*mesh->Nfp; 
  int Ntfp    = mesh->Nfaces*mesh->Nfp; 
  int intNtfp = mesh->Nfaces*mesh->intNfp;
  
  double flops;
  double bw;
  

  #if KERNEL_TEST==1
    flops = Nc*Np*8 + 4*Nc + Np*Nc*16 + Np*14 + Np*2;  // All float ops only
    bw = 2.0*Nbytes;
  #endif

  #if KERNEL_TEST==2
   flops = intNtfp*Nfp*16 + intNtfp*6 + intNtfp*1 + intNtfp*28 + Np*intNtfp*4;  // All float ops only      
   bw = 2.0*Nbytes;
  #endif
  
   #if KERNEL_TEST==3
   flops = 1*Np + 21*Ntfp + Np*(4*Ntfp + 8) +6*Ntfp+ Np*(4*Np + 2)+ Np*(1+2*Ntfp) + Np*(1+2*Np);  // All float ops only      
   bw = 2.0*Nbytes;
  #endif

  #if KERNEL_TEST==4
   flops = Np*(Np*2 + Np*2 + 6);  // All float ops only      
   bw = 2.0*Nbytes;
  #endif
  
  double gflops   = globalElements*flops*iterations/(1024*1024*1024.*globalElapsed);
  double d2dbound = copyBandwidth*flops/bw;
  double bwth     = 549*flops/bw;


  bw *= (globalElements*iterations)/(1024.*1024*1024*globalElapsed);



  if(rank==root){
    printf("[ RANKS N DOFS ELAPSEDTIME ITERATIONS (DOFS/RANKS) (DOFS/TIME/ITERATIONS/RANKS) (Kernel GFLOPS) (d2dbound GB/s) (copy GB/s) (achieved GP/s)]\n");
    printf("%02d %02d %02d %17.15lg %d %17.15E %17.15E %17.15E %17.15E %17.15E %17.15E\t\n",
           size, mesh->N, globalDofs, globalElapsed, iterations, globalDofs/(double)size,
           (globalDofs*iterations)/(globalElapsed*size), gflops, d2dbound, copyBandwidth, bw);

     printf("%02d\t%02d\t%17.15lg\t%17.15E\t%17.15E\n",
              mesh->N, globalDofs, globalElapsed, gflops, d2dbound);

    char fname[BUFSIZ];
    sprintf(fname, "KernelData.dat");
    FILE *fp;
    fp = fopen(fname, "a");

    fprintf(fp,"%02d\t%02d\t%17.15E\t%17.15E\t%17.15E\t%17.15E\t%17.15E\n",
              mesh->N, globalDofs, globalElapsed, gflops, d2dbound, bwth, bw);
    fclose(fp);




  }
  

  
 #endif 



  }


