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


// Initial conditions 
#define cdsFlowField3D(t,x,y,z, u,v,w) \
  {                                   \
    *(u) = p_ubar;                    \
    *(v) = p_vbar;                    \
    *(w) = p_wbar;                    \
  }   

#define cdsScalarField3D(t,x,y,z,s)	\
  {                                   \
    *(s) = p_sbar;                    \
  }   

// Boundary conditions
/* wall 1, inflow 2, outflow 3, x-slip 4, y-slip 5, z-slip 6 */
#define cdsDirichletConditions3D(bc, t, x, y, z, nx, ny, nz, sM, sB) \
{                                   \
  if(bc==1){                        \
    *(sB) = 0.f;                    \
  } else if(bc==2){                 \
    *(sB) = p_sbar;                 \
  } else if(bc==3){                 \
    *(sB) = sM;                     \
  } else if(bc==4||bc==5||bc==6){   \
    *(sB) = sM; \
  }                                 \
}

#define cdsNeumannConditions3D(bc, t, x, y, z, nx, ny, nz, sxM, syM, szM, sxB, syB, szB) \
{                                          \
  if(bc==1 || bc==2){                      \
    *(sxB) = sxM;                          \
    *(syB) = syM;                          \
    *(szB) = szM;                          \
  } else if(bc==3){                        \
    *(sxB) = 0.f;                          \
    *(syB) = 0.f;                          \
    *(szB) = 0.f;                          \
  } else if(bc==4||bc==5||bc==6){          \
    *(sxB) = nx*nx*sxM;                    \
    *(syB) = nx*nx*syM;                    \
    *(szB) = nx*nx*szM;                    \
  }                                        \
}