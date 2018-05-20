#include "bns.h"

void bnsReport(bns_t *bns, dfloat time, setupAide &options){

  mesh_t *mesh = bns->mesh; 
   
  // copy data back to host
  bns->o_q.copyTo(bns->q);


  // report ramp function
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if(rank==0){
    dfloat ramp, drampdt;
    bnsRampFunction(time, &ramp, &drampdt);
    printf("t: %g ramp: %g drampdt: %g\n", time, ramp, drampdt);
  }
  
  
  // not implemented yet
  #if 0 
    bnsForces(bns,t,options);
  #endif


  if(options.compareArgs("OUTPUT FILE FORMAT","VTU")){
    char fname[BUFSIZ];
    string outName;
    options.getArgs("OUTPUT FILE NAME", outName);
    sprintf(fname, "%s_%04d_%04d.vtu",(char*)outName.c_str(), rank, bns->frame++);
    bnsPlotVTU(bns, fname);
  }


  if(options.compareArgs("OUTPUT FILE FORMAT","TEC")){ 
    // //boltzmannComputeVorticity2D(mesh, mesh->q,5, mesh->Nfields);
    // char fname[BUFSIZ];
    // sprintf(fname, "foo_v2_%04d.dat",rank);
    // bnsPlotTEC(bns, fname, t);
  }
  
  
}