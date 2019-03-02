using MPI
using Pxest
using Pxest.p8est
using SparseArrays
using Adaptive
using LinearAlgebra

function dump_vtk(pxest;
                  mpicomm = MPI.COMM_WORLD,
                  mpirank = MPI.Comm_rank(mpicomm),
                  vtk_dir = "vtk_files",
                  vtk_base = "mesh_p8est",
                 )
  # dump VTK
  mpirank == 0 ? mkpath(vtk_dir) : nothing
  p8est.vtk_write_file(pxest, string(vtk_dir, "/", vtk_base))
  if mpirank == 0
    mv(string(vtk_dir, "/", vtk_base, ".pvtu"), string(vtk_base, ".pvtu"),
      force=true)
    mv(string(vtk_dir, "/", vtk_base, ".visit"), string(vtk_base, ".visit"),
      force=true)
  end
end

!MPI.Initialized() && MPI.Init()

let
  N = 3 # polynomial degree
  refine_level = 1

  conn = p8est.Connectivity(1,1,2)
  pxest = p8est.PXEST(conn; min_lvl=0)

  p8est.refine!(pxest) do which_tree, quadrant
    qid = ccall(:p8est_quadrant_child_id, Cint,
                (Ref{Pxest.p8est.pxest_quadrant_t},),
                quadrant)
    add = (qid == 0 || qid == 3 || qid == 5 || qid == 6) ? 1 : 0

    refine =  quadrant.level < refine_level + add

    refine ? Cint(1) : Cint(0)
  end
  p8est.balance!(pxest)
  p8est.partition!(pxest)
  p8est.ghost!(pxest)
  p8est.lnodes!(pxest, N)
  mesh = p8est.Mesh(pxest)

  # WARNING assume single MPI Rank
  S = sparse(1:length(mesh.DToC), mesh.DToC[:], ones(Int, length(mesh.DToC)))
  G = S'

  Snc = spzeros(size(S,1),size(S,1))

  for i in 1:mesh.Klocal
    Ie = nonconinterpolation(Float64, N, mesh.EToFC[i])

    idx = (i-1)*(N+1)^3 .+ (1:(N+1)^3)
    Snc[idx, idx] .= Ie
  end

  Gnc = Snc'

  r, w = lglpoints(Float64, N)
  D = spectralderivative(r)

  dump_vtk(pxest)
end

if !isinteractive()
  # Run gc to make sure cleanup happens before MPI is finalized
  GC.gc()
  MPI.Finalize()
end
