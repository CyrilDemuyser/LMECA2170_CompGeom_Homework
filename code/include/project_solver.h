/*
 * Fluid solver of the PFEM part of the project.
 *
 * ===========================================================================
 *  This solver is a compact version of the fluid solver of MigFlow:
 *      https://git.immc.ucl.ac.be/fluidparticles/migflow  --  www.migflow.be
 *  MigFlow is free software (GNU LGPL v3), (c) UCLouvain and Universite de
 *  Montpellier, see its AUTHORS file. The formulation, its stabilisation and
 *  the membrane surface tension come from MigFlow; see project_solver.c.
 * ===========================================================================
 *
 * It needs PETSc: run ./install_petsc.sh once, then cmake again. Without
 * PETSc everything else builds, but solve_navier_stokes() returns success = 0.
 */
#ifndef __PROJECT_SOLVER_H__
#define __PROJECT_SOLVER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BULK = 0,          // The node is "completely inside" the mesh
    FREE_SURFACE = 1,  // The node is on the boundary of the mesh but not in contact with a solid wall
    WALL = 2           // The node is on the boundary of the mesh and in contact with a solid wall
} node_type_et;

// Physical properties of the material
typedef struct {
    double density;      // rho [kg/m^3]
    double viscosity;    // mu, the DYNAMIC viscosity [Pa s] (water: 1e-3)
    double gravity[2];   // gravity in x, y [m/s^2]
    double sigma;        // surface tension coefficient [N/m] (water/air: 0.072), 0 for none
    double dt;           // time step [s]
} physical_props_st;

// Info about the solver that can be queried afterward
typedef struct {
    int success;           // 1 if the solve went fine, 0 otherwise (the reason is printed)
    int n_dofs;            // size of the linear system: 3*nnodes
    int ksp_iterations;    // iterations of the PETSc linear solver (1 for the default LU)
    double residual_norm;  // |A x - b| / |b| after the solve
    double assemble_time;  // CPU time spent assembling the system [s]
    double solve_time;     // CPU time spent solving it [s]
} solver_status_st;

// Contains the state of the solver (opaque: the PETSc objects live inside)
typedef struct solver_st solver_st;

/* Create a solver object. Contains the solver state/memory */
solver_st* solver_create(void);

/* Destroy a solver object
 *
 * @param solver [in]: the solver object to destroy
 */
void solver_destroy(solver_st* solver);

/* Calls the fluid solver: one implicit Euler time step of the incompressible
 * Navier-Stokes equations, on a mesh that moves with the fluid (PFEM: the
 * advection is done by moving the nodes, it is not in the equations).
 *
 * Assemble and solve the linear system corresponding to a given (@nodes, @elems) mesh.
 * The @node_type hint how we should treat nodes:
 *     - BULK: degrees of freedom in u, v, p
 *     - FREE_SURFACE: degrees of freedom in u, v, p. The surface is free of
 *       traction (p = 0 weakly) and pulled by the surface tension sigma.
 *     - WALL: fully constrained (no-slip) node, u=v=0.
 * If every boundary node is a WALL node (the fluid fills a closed box), the
 * pressure is fixed to 0 at one node.
 *
 * @param solver: the solver object (see solver_create)
 * @param nodes: the coordinates of the nodes, in row major.
 *               Size = 2*nnodes
 * @param nnodes: the number of nodes. Every node must belong to a triangle.
 * @param elems: the triangle elements, in row major.
 *               Size = 3*nelems
 * @param nelems: the number of triangle elements
 * @param node_type: the type of each node (bulk, free-surface, wall).
 *                   Size = nnodes
 * @param bnd_edges: ALL the edges on the boundary of the fluid mesh (the
 *                   surface tension acts on them), in row major, in any
 *                   orientation. Size = 2*n_bnd_edges.
 * @param n_bnd_edges: the number of edges on the boundary.
 * @param props: the physical properties of the simulation
 * @param uvp [IN/OUT]: the interleaved velocities/pressure (u1, v1, p1, u2, v2, p2, ...).
 *                      Allocated by the caller, Size=3*nnodes.
 *                      IN: the velocities of the previous time step (the
 *                          pressure is not used, the velocity of the WALL
 *                          nodes is taken as 0).
 *                      OUT: the new velocities and pressure.
 * @return status: the status of the solver
 */
solver_status_st solve_navier_stokes(
    solver_st* solver,
    double   const * nodes, uint32_t nnodes,
    uint32_t const * elems, uint32_t nelems,
    node_type_et const * node_type,
    uint32_t const * bnd_edges, uint32_t n_bnd_edges,
    physical_props_st const * props, double* uvp);

#ifdef __cplusplus
};
#endif

#endif // __PROJECT_SOLVER_H__
