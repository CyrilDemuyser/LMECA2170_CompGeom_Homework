"""
Python bindings of the C library of the project (libgeonum): this file lets
Python know how to call the C functions. Import it from your scripts:

    from lib import *

libgeonum is compiled by the CMake of the project, in code/build (set the
environment variable GEONUM_BUILD_DIR if you built it somewhere else). See
docs/python-bindings-examples/ for how these bindings work.
"""
import ctypes
import os
import pathlib

import numpy as np
from numpy import ctypeslib as npc

_build_dir = pathlib.Path(os.environ.get(
    "GEONUM_BUILD_DIR", pathlib.Path(__file__).resolve().parent / ".." / "build"))
if not list(_build_dir.glob("libgeonum.*")):
    raise FileNotFoundError(f"libgeonum not found in {_build_dir}: compile the project "
                            f"first (or set GEONUM_BUILD_DIR)")
geonum = npc.load_library("libgeonum", str(_build_dir))


# =============================================================================
#                   The fluid solver (include/project_solver.h)
# =============================================================================

# node_type_et: a C enum is an int
BULK, FREE_SURFACE, WALL = 0, 1, 2

# The same fields, in the same order, as the C structs
class PhysicalProps(ctypes.Structure):
    _fields_ = [
        ("density", ctypes.c_double),
        ("viscosity", ctypes.c_double),
        ("gravity", ctypes.c_double * 2),
        ("sigma", ctypes.c_double),
        ("dt", ctypes.c_double)
    ]

class SolverStatus(ctypes.Structure):
    _fields_ = [
        ("success", ctypes.c_int),
        ("n_dofs", ctypes.c_int),
        ("ksp_iterations", ctypes.c_int),
        ("residual_norm", ctypes.c_double),
        ("assemble_time", ctypes.c_double),
        ("solve_time", ctypes.c_double)
    ]

solver_create = geonum.solver_create
solver_create.restype = ctypes.c_void_p     # an opaque pointer: Python never looks inside
solver_create.argtypes = []

solver_destroy = geonum.solver_destroy
solver_destroy.restype = None
solver_destroy.argtypes = [ctypes.c_void_p]

solve_navier_stokes = geonum.solve_navier_stokes
solve_navier_stokes.restype = SolverStatus  # a struct returned by value
solve_navier_stokes.argtypes = [
    ctypes.c_void_p,                                                          # solver
    npc.ndpointer(dtype=np.float64, flags="C_CONTIGUOUS"), ctypes.c_uint32,   # nodes, nnodes
    npc.ndpointer(dtype=np.uint32, flags="C_CONTIGUOUS"), ctypes.c_uint32,    # elems, nelems
    npc.ndpointer(dtype=np.int32, flags="C_CONTIGUOUS"),                      # node_type
    npc.ndpointer(dtype=np.uint32, flags="C_CONTIGUOUS"), ctypes.c_uint32,    # bnd_edges, n_bnd_edges
    ctypes.POINTER(PhysicalProps),                                            # props
    npc.ndpointer(dtype=np.float64, flags=("C_CONTIGUOUS", "WRITEABLE"))      # uvp
]

class FluidSolver:
    """The C solver, created and freed automatically.

        solver = FluidSolver()
        props = PhysicalProps(density=1000, viscosity=1e-3, gravity=(0, -9.81),
                              sigma=0.072, dt=0.01)
        status = solver.solve(nodes, elems, node_type, bnd_edges, props, uvp)
    """
    def __init__(self):
        self._solver = solver_create()

    def __del__(self):
        if self._solver:
            solver_destroy(self._solver)
            self._solver = None

    def solve(self, nodes, elems, node_type, bnd_edges, props, uvp):
        """One time step, see solve_navier_stokes in project_solver.h.

        nodes (n,2), elems (m,3), node_type (n), bnd_edges (k,2): any array-like,
        converted to the C types. uvp: float64 numpy array of size 3n, modified in
        place: in, the velocities of the previous step; out, the new velocities
        and pressure (u0, v0, p0, u1, v1, p1, ...)."""
        nodes = np.ascontiguousarray(nodes, dtype=np.float64)
        elems = np.ascontiguousarray(elems, dtype=np.uint32)
        node_type = np.ascontiguousarray(node_type, dtype=np.int32)
        bnd_edges = np.ascontiguousarray(bnd_edges, dtype=np.uint32)
        nnodes = nodes.size // 2
        if node_type.size != nnodes or uvp.size != 3 * nnodes:
            raise ValueError(f"{nnodes} nodes: node_type needs {nnodes} values (has "
                             f"{node_type.size}) and uvp {3 * nnodes} (has {uvp.size})")
        return solve_navier_stokes(self._solver,
                                   nodes, nnodes,
                                   elems, elems.size // 3,
                                   node_type,
                                   bnd_edges, bnd_edges.size // 2,
                                   ctypes.byref(props), uvp)
