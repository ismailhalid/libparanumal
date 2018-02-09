#include "boltzmann2D.h"

void boltzmannMRABPmlSetup2D(mesh2D *mesh, char *options){

  //constant pml absorption coefficient
  dfloat xsigma  = 0.0;
  dfloat ysigma  = 0.0;
  // dfloat xsigma  = 100.*mesh->sqrtRT;
  // dfloat ysigma  = 100.*mesh->sqrtRT;
  //dfloat cxsigma = 200, cysigma = 200;
  dfloat q1    = 0.0, q2 = 1.0;    // Ramped profile coefficients 0<q1, 1>q2 , q1<exp<q2 or polynomial

// fifth order polynomialcoefficients for q1=0.2 and q2=0.8;
  dfloat c5 =  6.0 ;
  dfloat c4 = -15.0;
  dfloat c3 =  10.0;
  dfloat c2 =  0.0 ;
  dfloat c1 =  0.0 ;
  dfloat c0 =  0.0 ;
  // Try third order also
  // dfloat c5 =  0.0; // dummy
  // dfloat c4 =  0.0; // dummy
  // dfloat c3 = -2.0;
  // dfloat c2 =  3.0;
  // dfloat c1 =  0.0;
  // dfloat c0 =  0.0;



  //construct element and halo lists
  mesh->MRABpmlNelements = (iint *) calloc(mesh->MRABNlevels,sizeof(iint));
  mesh->MRABpmlElementIds = (iint **) calloc(mesh->MRABNlevels,sizeof(iint*));
  mesh->MRABpmlIds = (iint **) calloc(mesh->MRABNlevels, sizeof(iint*));

  mesh->MRABpmlNhaloElements = (iint *) calloc(mesh->MRABNlevels,sizeof(iint));
  mesh->MRABpmlHaloElementIds = (iint **) calloc(mesh->MRABNlevels,sizeof(iint*));
  mesh->MRABpmlHaloIds = (iint **) calloc(mesh->MRABNlevels, sizeof(iint*));


  //count the pml elements
  mesh->pmlNelements=0;
  for (iint lev =0;lev<mesh->MRABNlevels;lev++){
    for (iint m=0;m<mesh->MRABNelements[lev];m++) {
      iint e = mesh->MRABelementIds[lev][m];
      int type = mesh->elementInfo[e];
      if ((type==100)||(type==200)||(type==300)) {
        mesh->pmlNelements++;
        mesh->MRABpmlNelements[lev]++;
      }
    }
    for (iint m=0;m<mesh->MRABNhaloElements[lev];m++) {
      iint e = mesh->MRABhaloIds[lev][m];
      int type = mesh->elementInfo[e];
      if ((type==100)||(type==200)||(type==300))
        mesh->MRABpmlNhaloElements[lev]++;
    }
  }


  //set up the pml
  if (mesh->pmlNelements) {

    //construct a numbering of the pml elements
    iint *pmlIds = (iint *) calloc(mesh->Nelements,sizeof(iint));
    iint pmlcnt = 0;
    for (iint e=0;e<mesh->Nelements;e++) {
      int type = mesh->elementInfo[e];
      if ((type==100)||(type==200)||(type==300))  //pml element
        pmlIds[e] = pmlcnt++;
    }

    //set up lists of pml elements and remove the pml elements from the nonpml MRAB lists
    for (iint lev =0;lev<mesh->MRABNlevels;lev++){
      mesh->MRABpmlElementIds[lev] = (iint *) calloc(mesh->MRABpmlNelements[lev],sizeof(iint));
      mesh->MRABpmlIds[lev] = (iint *) calloc(mesh->MRABpmlNelements[lev],sizeof(iint));
      mesh->MRABpmlHaloElementIds[lev] = (iint *) calloc(mesh->MRABpmlNhaloElements[lev],sizeof(iint));
      mesh->MRABpmlHaloIds[lev] = (iint *) calloc(mesh->MRABpmlNhaloElements[lev],sizeof(iint));

      iint pmlcnt = 0;
      iint nonpmlcnt = 0;
      for (iint m=0;m<mesh->MRABNelements[lev];m++){
        iint e = mesh->MRABelementIds[lev][m];
        int type = mesh->elementInfo[e];

        if ((type==100)||(type==200)||(type==300)) { //pml element
          mesh->MRABpmlElementIds[lev][pmlcnt] = e;
          mesh->MRABpmlIds[lev][pmlcnt] = pmlIds[e];
          pmlcnt++;
        } else { //nonpml element
          mesh->MRABelementIds[lev][nonpmlcnt] = e;
          nonpmlcnt++;
        }
      }

      pmlcnt = 0;
      nonpmlcnt = 0;
      for (iint m=0;m<mesh->MRABNhaloElements[lev];m++){
        iint e = mesh->MRABhaloIds[lev][m];
        int type = mesh->elementInfo[e];

        if ((type==100)||(type==200)||(type==300)) { //pml element
          mesh->MRABpmlHaloElementIds[lev][pmlcnt] = e;
          mesh->MRABpmlHaloIds[lev][pmlcnt] = pmlIds[e];
          pmlcnt++;
        } else { //nonpml element
          mesh->MRABhaloIds[lev][nonpmlcnt] = e;
          nonpmlcnt++;
        }
      }

      //resize nonpml element lists
      mesh->MRABNelements[lev] -= mesh->MRABpmlNelements[lev];
      mesh->MRABNhaloElements[lev] -= mesh->MRABpmlNhaloElements[lev];
      mesh->MRABelementIds[lev] = (iint*) realloc(mesh->MRABelementIds[lev],mesh->MRABNelements[lev]*sizeof(iint));
      mesh->MRABhaloIds[lev]    = (iint*) realloc(mesh->MRABhaloIds[lev],mesh->MRABNhaloElements[lev]*sizeof(iint));
    }



    iint Nnodes = 0;
    if(strstr(options,"CUBATURE")){ // !!!!!!!!!!!!!!!
      //
      printf("Setting PML Coefficient for Cubature Integration\n");
      //set up damping parameter
      mesh->pmlSigmaX = (dfloat *) calloc(mesh->pmlNelements*mesh->cubNp,sizeof(dfloat));
      mesh->pmlSigmaY = (dfloat *) calloc(mesh->pmlNelements*mesh->cubNp,sizeof(dfloat));  
      Nnodes = mesh->cubNp;    
    }
     else{
      printf("Setting PML Coefficients for Nodal Collocation Integration\n");
      //set up damping parameter
      mesh->pmlSigmaX = (dfloat *) calloc(mesh->pmlNelements*mesh->Np,sizeof(dfloat));
      mesh->pmlSigmaY = (dfloat *) calloc(mesh->pmlNelements*mesh->Np,sizeof(dfloat));
      Nnodes = mesh->Np; 
    }




    //find the bounding box of the whole domain and interior domain
    dfloat xmin = 1e9, xmax =-1e9;
    dfloat ymin = 1e9, ymax =-1e9;
    dfloat pmlxmin = 1e9, pmlxmax =-1e9;
    dfloat pmlymin = 1e9, pmlymax =-1e9;
    for (iint e=0;e<mesh->Nelements;e++) {
      for (int n=0;n<mesh->Nverts;n++) {
        dfloat x = mesh->EX[e*mesh->Nverts+n];
        dfloat y = mesh->EY[e*mesh->Nverts+n];

        pmlxmin = (pmlxmin > x) ? x : pmlxmin;
        pmlymin = (pmlymin > y) ? y : pmlymin;
        pmlxmax = (pmlxmax < x) ? x : pmlxmax;
        pmlymax = (pmlymax < y) ? y : pmlymax;
      }

      //skip pml elements
      int type = mesh->elementInfo[e];
      if ((type==100)||(type==200)||(type==300)) continue;

      for (int n=0;n<mesh->Nverts;n++) {
        dfloat x = mesh->EX[e*mesh->Nverts+n];
        dfloat y = mesh->EY[e*mesh->Nverts+n];

        xmin = (xmin > x) ? x : xmin;
        ymin = (ymin > y) ? y : ymin;
        xmax = (xmax < x) ? x : xmax;
        ymax = (ymax < y) ? y : ymax;
      }
    }

    iint order = 2; 
    //
    if(strstr(options,"CONSTANT"))
      order = 0; 
    if(strstr(options,"LINEAR"))
      order = 1; 
    if(strstr(options,"QUADRATIC"))
      order = 2;
    if(strstr(options,"FORTHORDER"))
      order = 4;
    if(strstr(options, "ERFFUNCTION"))
      order =1;


    dfloat xmaxScale = pow(pmlxmax-xmax,order);
    dfloat xminScale = pow(pmlxmin-xmin,order);
    dfloat ymaxScale = pow(pmlymax-ymax,order);
    dfloat yminScale = pow(pmlymin-ymin,order);

    //set up the damping factor
    for (iint lev =0;lev<mesh->MRABNlevels;lev++){
      for (iint m=0;m<mesh->MRABpmlNelements[lev];m++) {
        iint e = mesh->MRABpmlElementIds[lev][m];
        iint pmlId = mesh->MRABpmlIds[lev][m];
        int type = mesh->elementInfo[e];

        iint id = e*mesh->Nverts;

        dfloat xe1 = mesh->EX[id+0]; /* x-coordinates of vertices */
        dfloat xe2 = mesh->EX[id+1];
        dfloat xe3 = mesh->EX[id+2];

        dfloat ye1 = mesh->EY[id+0]; /* y-coordinates of vertices */
        dfloat ye2 = mesh->EY[id+1];
        dfloat ye3 = mesh->EY[id+2];

         for(iint n=0;n<Nnodes;++n){ /* for each node */
           dfloat x = 0, y = 0; 
          if(Nnodes==mesh->cubNp){
            dfloat rn = mesh->cubr[n];
            dfloat sn = mesh->cubs[n]; 
            x = -0.5*(rn+sn)*xe1 + 0.5*(1+rn)*xe2 + 0.5*(1+sn)*xe3;
            y = -0.5*(rn+sn)*ye1 + 0.5*(1+rn)*ye2 + 0.5*(1+sn)*ye3;
          }
          else{
            x = mesh->x[n + e*mesh->Np];
            y = mesh->y[n + e*mesh->Np];
          }




        if(!strstr(options,"SMOOTHPOLYNOMIAL")){
          if (type==100) { //X Pml
            if(x>xmax)
              mesh->pmlSigmaX[Nnodes*pmlId + n] = xsigma*pow(x-xmax,order)/xmaxScale;
            if(x<xmin)
              mesh->pmlSigmaX[Nnodes*pmlId + n] = xsigma*pow(x-xmin,order)/xminScale;
          } else if (type==200) { //Y Pml
            if(y>ymax)
              mesh->pmlSigmaY[Nnodes*pmlId + n] = ysigma*pow(y-ymax,order)/ymaxScale;
            if(y<ymin)
              mesh->pmlSigmaY[Nnodes*pmlId + n] = ysigma*pow(y-ymin,order)/yminScale;
          } else if (type==300) { //XY Pml
            if(x>xmax)
              mesh->pmlSigmaX[Nnodes*pmlId + n] = xsigma*pow(x-xmax,order)/xmaxScale;
            if(x<xmin)
              mesh->pmlSigmaX[Nnodes*pmlId + n] = xsigma*pow(x-xmin,order)/xminScale;
            if(y>ymax)
              mesh->pmlSigmaY[Nnodes*pmlId + n] = ysigma*pow(y-ymax,order)/ymaxScale;
            if(y<ymin)
              mesh->pmlSigmaY[Nnodes*pmlId + n] = ysigma*pow(y-ymin,order)/yminScale;
          }
#if 0

           if (type==100) { //X Pml
            if(x>xmax)
              mesh->pmlSigmaX[Nnodes*pmlId + n] += 0.5*pow(x-xmax,order)/xmaxScale*mesh->pmlSigmaY[Nnodes*pmlId + n];
            if(x<xmin)
              mesh->pmlSigmaX[Nnodes*pmlId + n] += 0.5*pow(x-xmin,order)/xminScale*mesh->pmlSigmaY[Nnodes*pmlId + n];
          } else if (type==200) { //Y Pml
            if(y>ymax)
              mesh->pmlSigmaY[Nnodes*pmlId + n] += 0.5*pow(y-ymax,order)/ymaxScale*mesh->pmlSigmaX[Nnodes*pmlId + n];
            if(y<ymin)
              mesh->pmlSigmaY[Nnodes*pmlId + n] += 0.5*pow(y-ymin,order)/yminScale*mesh->pmlSigmaX[Nnodes*pmlId + n];
          } else if (type==300) { //XY Pml
            if(x>xmax)
              mesh->pmlSigmaX[Nnodes*pmlId + n] += 0.5*pow(x-xmax,order)/xmaxScale*mesh->pmlSigmaY[Nnodes*pmlId + n];
            if(x<xmin)
              mesh->pmlSigmaX[Nnodes*pmlId + n] += 0.5*pow(x-xmin,order)/xminScale*mesh->pmlSigmaY[Nnodes*pmlId + n];
            if(y>ymax)
              mesh->pmlSigmaY[Nnodes*pmlId + n] += 0.5*pow(y-ymax,order)/ymaxScale*mesh->pmlSigmaX[Nnodes*pmlId + n];
            if(y<ymin)
              mesh->pmlSigmaY[Nnodes*pmlId + n] += 0.5*pow(y-ymin,order)/yminScale*mesh->pmlSigmaX[Nnodes*pmlId + n];
          }

#endif




        }

        else if(strstr(options,"SMOOTHPOLYNOMIAL")){

          dfloat qx=0,      qy = 0;
          dfloat taux = 0,  tauy = 0;
          dfloat polyx = 0, polyy = 0;

          if (type==100) { //X Pml
            if(x>xmax)
              qx   = (x-xmax)/(pmlxmax-xmax);
            if(x<xmin)
              qx    = (x-xmin)/(pmlxmin-xmin);
          } else if (type==200) { //Y Pml
            if(y>ymax)
              qy    = (y-ymax)/(pmlymax-ymax);
            if(y<ymin)
              qy    = (y-ymin)/(pmlymin-ymin);
          } else if (type==300) { //XY Pml
            if(x>xmax)
             qx   = (x-xmax)/(pmlxmax-xmax);
            if(x<xmin)
             qx    = (x-xmin)/(pmlxmin-xmin);
            if(y>ymax)
             qy    = (y-ymax)/(pmlymax-ymax);
            if(y<ymin)
             qy    = (y-ymin)/(pmlymin-ymin);
          }
          // Third Order
          taux  =  (qx-q1)/(q2-q1);
          tauy  =  (qy-q1)/(q2-q1);
          // Fifth Order
          polyx = c5*pow(taux,5) + c4*pow(taux,4) +c3*pow(taux,3) + c2*pow(taux,2) + c1*taux + c0;
          polyy = c5*pow(tauy,5) + c4*pow(tauy,4) +c3*pow(tauy,3) + c2*pow(tauy,2) + c1*tauy + c0;

          polyx = qx<=q1 ? 0. : polyx;  polyx = qx>=q2 ? 1. : polyx;
          polyy = qy<=q1 ? 0. : polyy;  polyy = qy>=q2 ? 1. : polyy;
           
          if (type==100) { //X Pml
             mesh->pmlSigmaX[Nnodes*pmlId + n] = xsigma*polyx;
          } else if (type==200) { //Y Pml
             mesh->pmlSigmaY[Nnodes*pmlId + n] = ysigma*polyy;
          } else if (type==300) { //XY Pml
            if(x>xmax || x<xmin)
             mesh->pmlSigmaX[Nnodes*pmlId + n] = xsigma*polyx;
            if(y>ymax || y<ymin)
             mesh->pmlSigmaY[Nnodes*pmlId + n] = ysigma*polyy;
          }
        }


        }
      }
    }

    printf("PML: found %d elements inside absorbing layers and %d elements outside\n",
    mesh->pmlNelements, mesh->Nelements-mesh->pmlNelements);

    mesh->pmlqx    = (dfloat*) calloc(mesh->pmlNelements*mesh->Np*mesh->Nfields, sizeof(dfloat));
    mesh->pmlrhsqx = (dfloat*) calloc(mesh->Nrhs*mesh->pmlNelements*mesh->Np*mesh->Nfields, sizeof(dfloat));

    mesh->pmlqy    = (dfloat*) calloc(mesh->pmlNelements*mesh->Np*mesh->Nfields, sizeof(dfloat));
    mesh->pmlrhsqy = (dfloat*) calloc(mesh->Nrhs*mesh->pmlNelements*mesh->Np*mesh->Nfields, sizeof(dfloat));


    // set up PML on DEVICE    
    mesh->o_pmlqx     = mesh->device.malloc(mesh->pmlNelements*mesh->Np*mesh->Nfields*sizeof(dfloat), mesh->pmlqx);
    mesh->o_pmlrhsqx  = mesh->device.malloc(mesh->Nrhs*mesh->pmlNelements*mesh->Np*mesh->Nfields*sizeof(dfloat), mesh->pmlrhsqx);

    mesh->o_pmlqy     = mesh->device.malloc(mesh->pmlNelements*mesh->Np*mesh->Nfields*sizeof(dfloat), mesh->pmlqy);
    mesh->o_pmlrhsqy  = mesh->device.malloc(mesh->Nrhs*mesh->pmlNelements*mesh->Np*mesh->Nfields*sizeof(dfloat), mesh->pmlrhsqy);


     mesh->o_pmlSigmaX     = mesh->device.malloc(mesh->pmlNelements*Nnodes*sizeof(dfloat),mesh->pmlSigmaX);
     mesh->o_pmlSigmaY     = mesh->device.malloc(mesh->pmlNelements*Nnodes*sizeof(dfloat),mesh->pmlSigmaY);



    mesh->o_MRABpmlElementIds     = (occa::memory *) malloc(mesh->MRABNlevels*sizeof(occa::memory));
    mesh->o_MRABpmlIds            = (occa::memory *) malloc(mesh->MRABNlevels*sizeof(occa::memory));
    mesh->o_MRABpmlHaloElementIds = (occa::memory *) malloc(mesh->MRABNlevels*sizeof(occa::memory));
    mesh->o_MRABpmlHaloIds        = (occa::memory *) malloc(mesh->MRABNlevels*sizeof(occa::memory));
    for (iint lev=0;lev<mesh->MRABNlevels;lev++) {
      if (mesh->MRABpmlNelements[lev]) {
        mesh->o_MRABpmlElementIds[lev] = mesh->device.malloc(mesh->MRABpmlNelements[lev]*sizeof(iint),
           mesh->MRABpmlElementIds[lev]);
        mesh->o_MRABpmlIds[lev] = mesh->device.malloc(mesh->MRABpmlNelements[lev]*sizeof(iint),
           mesh->MRABpmlIds[lev]);
      }
      if (mesh->MRABpmlNhaloElements[lev]) {
        mesh->o_MRABpmlHaloElementIds[lev] = mesh->device.malloc(mesh->MRABpmlNhaloElements[lev]*sizeof(iint),
           mesh->MRABpmlHaloElementIds[lev]);
        mesh->o_MRABpmlHaloIds[lev] = mesh->device.malloc(mesh->MRABpmlNhaloElements[lev]*sizeof(iint),
           mesh->MRABpmlHaloIds[lev]);
      }
    }

    free(pmlIds);
  }



// for (iint lev =0;lev<mesh->MRABNlevels;lev++){
//   for (iint m=0;m<mesh->MRABpmlNelements[lev];m++) {
//         iint e = mesh->MRABpmlElementIds[lev][m];
//         iint pmlId = mesh->MRABpmlIds[lev][m];

//     for(iint n=0;n<mesh->Np;n++){
//       mesh->q[mesh->Nfields*(n + e*mesh->Np) + 0] = mesh->pmlSigmaX[mesh->Np*pmlId + n];
//       mesh->q[mesh->Nfields*(n + e*mesh->Np) + 1] = mesh->pmlSigmaY[mesh->Np*pmlId + n];
//     }
//   }
// }
  
 
  // char fname[BUFSIZ];
  // sprintf(fname, "pmlProfile1.vtu");
  // boltzmannPlotVTU2D(mesh, fname);


}
