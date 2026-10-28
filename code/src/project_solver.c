/*
 * Fluid solver of the PFEM part of the project.
 *
 * ===========================================================================
 *  ADAPTED FROM MIGFLOW
 *      https://git.immc.ucl.ac.be/fluidparticles/migflow  --  www.migflow.be
 *
 *  MigFlow - Copyright (C) <2010-2026>
 *  <Universite catholique de Louvain (UCL), Belgium
 *   Universite de Montpellier, France>
 *  List of the contributors to the development of MigFlow: see its AUTHORS
 *  file.
 *
 *  This file is a compact rewrite of parts of the fluid solver of MigFlow,
 *  restricted to one fluid, P1-P1 elements and no advection:
 *    - src/fluid/fluid_problem.c: f_volume (the volume terms and their PSPG
 *      stabilisation), the open boundary of f_boundary (the free surface),
 *      fluid_problem_compute_stab_parameters and mesh_element_size;
 *    - src/fluid/surface_tension.c: membrane_face (the surface tension).
 *  Like MigFlow, this file is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version. It is distributed in the
 *  hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU Lesser General Public License for more details:
 *  <http://www.gnu.org/licenses/>.
 * ===========================================================================
 *
 * You do not need to understand this file to do the project, but here is what
 * it solves, for the curious.
 *
 * Unknowns: u, v, p at every node, linear on each triangle (P1-P1), stored
 * like uvp: the dofs of node i are 3i (u), 3i+1 (v) and 3i+2 (p).
 *
 * The mesh moves with the fluid (that is the whole point of PFEM), so there
 * is no advection term: one implicit Euler step is the LINEAR problem, for all
 * test functions v and q,
 *
 *   int rho (u - u_old)/dt . v + int mu (grad u + grad u^T) : grad v
 *     - int p div v - int rho g . v + [surface tension] = 0
 *
 *   int q div u + sum_e int_e tau/rho (rho (u - u_old)/dt + grad p - rho g) . grad q = 0
 *
 * - The pressure is integrated by parts: on the boundary, the natural
 *   condition is a zero traction, (-p I + mu (grad u + grad u^T)) n = 0, the
 *   free surface (in MigFlow: an open boundary with p = 0, no viscous term).
 * - The no-slip walls replace the rows of the velocity of the WALL nodes by
 *   u = v = 0.
 * - Equal order P1-P1 elements are not stable on their own: the pressure would
 *   oscillate. The second sum (PSPG, pressure stabilizing Petrov-Galerkin) adds
 *   the residual of the momentum equation, tested with grad q, to the
 *   continuity equation. tau = [(2/dt)^2 + (4 nu/h^2)^2]^(-1/2), nu = mu/rho,
 *   h = 2 sqrt(area/pi). The residual vanishes for the exact solution, so the
 *   stabilisation does not change it: a fluid at rest has exactly the
 *   hydrostatic pressure.
 * - Surface tension, the "membrane" approach: each boundary edge [a,b] of
 *   length L stores the energy sigma L, and its derivative is a force on the
 *   two nodes, sigma t on a and -sigma t on b (t the unit vector from a to b):
 *   the boundary acts as a stretched rubber band. It acts on x + theta dt u
 *   (where the nodes will be after the step), linearized, which adds
 *   theta dt sigma/L (I - t t^T) (u_a - u_b) on a (and the opposite on b).
 *   theta = 1 makes it stable whatever dt.
 *
 * The linear system is solved by PETSc, by default with a direct sparse LU.
 * Its options have the prefix "fluid_" and can be changed without recompiling,
 * with the environment variable PETSC_OPTIONS, for instance
 *     PETSC_OPTIONS="-fluid_ksp_type gmres -fluid_pc_type ilu -fluid_ksp_monitor"
 */
#include "project_solver.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_PETSC
#include <math.h>
#include <petscksp.h>
#include <time.h>
#endif

struct solver_st {
#ifdef HAVE_PETSC
    KSP ksp;
#else
    int unused;
#endif
};


#ifndef HAVE_PETSC
/* ========================================================================== */
/*  Without PETSc: everything builds, but there is no fluid solver           */
/* ========================================================================== */

solver_st* solver_create(void) {
    return calloc(1, sizeof(solver_st));
}

void solver_destroy(solver_st* solver) {
    free(solver);
}

solver_status_st solve_navier_stokes(
    solver_st* solver,
    double   const * nodes, uint32_t nnodes,
    uint32_t const * elems, uint32_t nelems,
    node_type_et const * node_type,
    uint32_t const * bnd_edges, uint32_t n_bnd_edges,
    physical_props_st const * props, double* uvp) {

    (void) solver; (void) nodes; (void) nnodes; (void) elems; (void) nelems;
    (void) node_type; (void) bnd_edges; (void) n_bnd_edges; (void) props;
    (void) uvp;
    fprintf(stderr, "[solver] the fluid solver needs PETSc: run "
                    "./install_petsc.sh, then cmake again\n");
    return (solver_status_st) {0};
}

#else
/* ========================================================================== */
/*  Element contributions                                                     */
/* ========================================================================== */

#define PI 3.14159265358979323846       // M_PI is not in C11 with -pedantic
#define THETA 1.0                       // implicitness of the surface tension

static double pow2(double a) { return a * a; }

// Area of triangle t, and the (constant) gradients of its 3 shape functions.
// Works for both orientations, returns 0 for a flat triangle.
static double triangleGeometry(const double* nodes, const uint32_t* t,
                               double dphi[3][2])
{
    const double* x0 = &nodes[2 * t[0]];
    const double* x1 = &nodes[2 * t[1]];
    const double* x2 = &nodes[2 * t[2]];
    double det = (x1[0] - x0[0]) * (x2[1] - x0[1])
               - (x2[0] - x0[0]) * (x1[1] - x0[1]);
    if (det == 0.0) return 0.0;

    dphi[0][0] = (x1[1] - x2[1]) / det;  dphi[0][1] = (x2[0] - x1[0]) / det;
    dphi[1][0] = (x2[1] - x0[1]) / det;  dphi[1][1] = (x0[0] - x2[0]) / det;
    dphi[2][0] = (x0[1] - x1[1]) / det;  dphi[2][1] = (x1[0] - x0[0]) / det;
    return 0.5 * fabs(det);
}

/*
 * The 9x9 matrix K and the right-hand side F of one triangle, local dof
 * 3*i + c is the field c (u, v, p) of its node i. Every integrand is linear or
 * quadratic, so everything is computed exactly:
 *     int phi_a phi_b = area (1 + delta_ab) / 12,     int phi_a = area / 3.
 */
static void elementSystem(const double* nodes, const uint32_t* t,
                          const double* uvpOld, const node_type_et* nodeType,
                          const physical_props_st* props,
                          double K[9][9], double F[9])
{
    double dphi[3][2];
    const double area = triangleGeometry(nodes, t, dphi);
    const double rho = props->density, mu = props->viscosity, dt = props->dt;
    const double* g = props->gravity;

    const double h = 2.0 * sqrt(area / PI);
    const double tau = 1.0 / sqrt(pow2(2.0 / dt) + pow2(4.0 * mu / rho / (h * h)));

    // the old velocity, 0 on the walls (a node may just have hit one)
    double uOld[3][2], uMean[2] = {0.0, 0.0};
    for (int a = 0; a < 3; a++)
        for (int d = 0; d < 2; d++) {
            uOld[a][d] = nodeType[t[a]] == WALL ? 0.0 : uvpOld[3 * t[a] + d];
            uMean[d] += uOld[a][d] / 3.0;
        }

    for (int i = 0; i < 9; i++) {
        F[i] = 0.0;
        for (int j = 0; j < 9; j++) K[i][j] = 0.0;
    }

    for (int a = 0; a < 3; a++) {
        const int pa = 3 * a + 2;               // the continuity row of node a

        for (int b = 0; b < 3; b++) {
            const int pb = 3 * b + 2;
            const double mass = area * (a == b ? 2.0 : 1.0) / 12.0;
            const double gradGrad = dphi[a][0] * dphi[b][0] + dphi[a][1] * dphi[b][1];

            for (int d = 0; d < 2; d++) {
                const int ua = 3 * a + d, ub = 3 * b + d;

                // inertia, rho (u - u_old)/dt . v
                K[ua][ub] += rho / dt * mass;
                F[ua]     += rho / dt * mass * uOld[b][d];

                // viscosity, mu (grad u + grad u^T) : grad v
                for (int c = 0; c < 2; c++)
                    K[ua][3 * b + c] += mu * area * ((d == c ? gradGrad : 0.0)
                                                     + dphi[b][d] * dphi[a][c]);

                // pressure, - p div v
                K[ua][pb] -= area / 3.0 * dphi[a][d];

                // continuity, q div u
                K[pa][ub] += area / 3.0 * dphi[b][d];

                // PSPG, the inertia: tau/dt (u - u_old) . grad q
                K[pa][ub] += tau / dt * area / 3.0 * dphi[a][d];
            }
            // PSPG, the pressure gradient: tau/rho grad p . grad q
            K[pa][pb] += tau / rho * area * gradGrad;
        }

        for (int d = 0; d < 2; d++) {
            // gravity, rho g . v
            F[3 * a + d] += rho * g[d] * area / 3.0;
            // PSPG, the known part: tau (u_old/dt + g) . grad q
            F[pa] += tau * area * (uMean[d] / dt + g[d]) * dphi[a][d];
        }
    }
}

/*
 * Surface tension of one boundary edge [xa, xb] (MigFlow's membrane_face):
 * the 4x4 matrix K and right-hand side F on the dofs (u_a, v_a, u_b, v_b).
 */
static void membraneEdge(const double* xa, const double* xb, double sigma,
                         double dt, double K[4][4], double F[4])
{
    double t[2] = {xb[0] - xa[0], xb[1] - xa[1]};
    const double length = sqrt(t[0] * t[0] + t[1] * t[1]);
    t[0] /= length;
    t[1] /= length;

    const double k = THETA * dt * sigma / length;
    for (int d = 0; d < 2; d++) {
        F[d]     =  sigma * t[d];               // pulls a towards b
        F[2 + d] = -sigma * t[d];               // and b towards a
        for (int e = 0; e < 2; e++) {
            const double kp = k * ((d == e ? 1.0 : 0.0) - t[d] * t[e]);
            K[d][e]         =  kp;
            K[2 + d][2 + e] =  kp;
            K[d][2 + e]     = -kp;
            K[2 + d][e]     = -kp;
        }
    }
}


/* ========================================================================== */
/*  Checks                                                                    */
/* ========================================================================== */

// Prints what is wrong and returns 0 if the input cannot be solved.
static int checkInput(const double* nodes, uint32_t nnodes,
                      const uint32_t* elems, uint32_t nelems,
                      const uint32_t* bndEdges, uint32_t nBndEdges,
                      const physical_props_st* props, int* nTriAtNode)
{
    if (!(props->density > 0.0) || !(props->dt > 0.0) ||
        !(props->viscosity >= 0.0) || !(props->sigma >= 0.0)) {
        fprintf(stderr, "[solver] invalid physical properties: density and dt "
                        "must be > 0, viscosity and sigma >= 0\n");
        return 0;
    }
    if (nelems == 0) {
        fprintf(stderr, "[solver] the mesh has no triangle\n");
        return 0;
    }
    for (uint32_t v = 0; v < nnodes; v++) nTriAtNode[v] = 0;
    for (uint32_t e = 0; e < nelems; e++) {
        const uint32_t* t = &elems[3 * e];
        for (int i = 0; i < 3; i++) {
            if (t[i] >= nnodes) {
                fprintf(stderr, "[solver] triangle %u uses node %u, but there "
                                "are only %u nodes\n", e, t[i], nnodes);
                return 0;
            }
            nTriAtNode[t[i]]++;
        }
        double dphi[3][2];
        if (triangleGeometry(nodes, t, dphi) == 0.0) {
            fprintf(stderr, "[solver] triangle %u (%u, %u, %u) is flat\n",
                    e, t[0], t[1], t[2]);
            return 0;
        }
    }
    for (uint32_t v = 0; v < nnodes; v++) {
        if (nTriAtNode[v] == 0) {
            fprintf(stderr, "[solver] node %u belongs to no triangle: remove it "
                            "before calling the solver\n", v);
            return 0;
        }
    }
    for (uint32_t i = 0; i < 2 * nBndEdges; i++) {
        if (bndEdges[i] >= nnodes) {
            fprintf(stderr, "[solver] boundary edge %u uses node %u, but there "
                            "are only %u nodes\n", i / 2, bndEdges[i], nnodes);
            return 0;
        }
    }
    return 1;
}


/* ========================================================================== */
/*  PETSc                                                                     */
/* ========================================================================== */

// Returns the PETSc error code of `call` if it fails
#define TRY(call) do { PetscErrorCode err_ = (call); if (err_) return err_; } while (0)

static int nSolvers = 0;                // solvers alive
static int petscStartedHere = 0;        // PETSc was initialized by solver_create

// Sets the option -fluid_<name> to `value`, unless the user already set it
static PetscErrorCode setDefaultOption(const char* name, const char* value)
{
    char full[64];
    PetscBool set;
    snprintf(full, sizeof full, "-fluid_%s", name);
    TRY(PetscOptionsHasName(NULL, NULL, full, &set));
    if (!set) TRY(PetscOptionsSetValue(NULL, full, value));
    return 0;
}

static PetscErrorCode createKsp(KSP* ksp)
{
    PetscBool initialized;
    TRY(PetscInitialized(&initialized));
    if (!initialized) {
        TRY(PetscInitializeNoArguments());
        petscStartedHere = 1;
    }
    TRY(setDefaultOption("ksp_type", "preonly"));
    TRY(setDefaultOption("pc_type", "lu"));
    TRY(KSPCreate(PETSC_COMM_SELF, ksp));
    TRY(KSPSetOptionsPrefix(*ksp, "fluid_"));
    TRY(KSPSetFromOptions(*ksp));
    return 0;
}

solver_st* solver_create(void) {
    solver_st* solver = calloc(1, sizeof(solver_st));
    if (createKsp(&solver->ksp)) {
        fprintf(stderr, "[solver] could not initialize PETSc\n");
        solver->ksp = NULL;
    }
    nSolvers++;
    return solver;
}

void solver_destroy(solver_st* solver) {
    if (solver == NULL) return;
    if (solver->ksp != NULL) KSPDestroy(&solver->ksp);
    free(solver);
    if (--nSolvers == 0 && petscStartedHere) {
        PetscFinalize();
        petscStartedHere = 0;
    }
}

static PetscErrorCode assembleAndSolve(
    KSP ksp, const double* nodes, uint32_t nnodes,
    const uint32_t* elems, uint32_t nelems, const node_type_et* nodeType,
    const uint32_t* bndEdges, uint32_t nBndEdges,
    const physical_props_st* props, const int* nTriAtNode,
    double* uvp, solver_status_st* status)
{
    const PetscInt n = 3 * (PetscInt) nnodes;
    clock_t start = clock();

    /* ---- the matrix: a node is coupled to itself and to its neighbours,
     *      at most 2 per triangle around it ---- */
    Mat A;
    PetscInt* nnz = malloc((size_t) n * sizeof(PetscInt));
    for (uint32_t v = 0; v < nnodes; v++)
        for (int c = 0; c < 3; c++)
            nnz[3 * v + c] = PetscMin(n, 3 * (1 + 2 * (PetscInt) nTriAtNode[v]));
    PetscErrorCode err = MatCreateSeqAIJ(PETSC_COMM_SELF, n, n, 0, nnz, &A);
    free(nnz);
    TRY(err);

    Vec b, x;
    TRY(VecCreateSeq(PETSC_COMM_SELF, n, &b));
    TRY(VecDuplicate(b, &x));
    TRY(VecSet(b, 0.0));

    /* ---- the triangles ---- */
    for (uint32_t e = 0; e < nelems; e++) {
        const uint32_t* t = &elems[3 * e];
        double K[9][9], F[9];
        elementSystem(nodes, t, uvp, nodeType, props, K, F);

        PetscInt idx[9];
        for (int i = 0; i < 3; i++)
            for (int c = 0; c < 3; c++)
                idx[3 * i + c] = 3 * (PetscInt) t[i] + c;
        TRY(MatSetValues(A, 9, idx, 9, idx, &K[0][0], ADD_VALUES));
        TRY(VecSetValues(b, 9, idx, F, ADD_VALUES));
    }

    /* ---- the surface tension, on the boundary edges ---- */
    for (uint32_t i = 0; i < nBndEdges && props->sigma > 0.0; i++) {
        const uint32_t na = bndEdges[2 * i], nb = bndEdges[2 * i + 1];
        if (na == nb) continue;
        double K[4][4], F[4];
        membraneEdge(&nodes[2 * na], &nodes[2 * nb], props->sigma, props->dt, K, F);

        PetscInt idx[4] = {3 * (PetscInt) na, 3 * (PetscInt) na + 1,
                           3 * (PetscInt) nb, 3 * (PetscInt) nb + 1};
        TRY(MatSetValues(A, 4, idx, 4, idx, &K[0][0], ADD_VALUES));
        TRY(VecSetValues(b, 4, idx, F, ADD_VALUES));
    }

    TRY(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
    TRY(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
    TRY(VecAssemblyBegin(b));
    TRY(VecAssemblyEnd(b));

    /* ---- the strong conditions: u = v = 0 on the walls, and p = 0 at one
     *      node when no boundary node is free (closed box) ---- */
    PetscInt* rows = malloc((size_t) (2 * nnodes + 1) * sizeof(PetscInt));
    PetscInt nRows = 0;
    for (uint32_t v = 0; v < nnodes; v++) {
        if (nodeType[v] != WALL) continue;
        rows[nRows++] = 3 * (PetscInt) v;
        rows[nRows++] = 3 * (PetscInt) v + 1;
    }
    int freeBoundary = (nBndEdges == 0);
    for (uint32_t i = 0; i < 2 * nBndEdges; i++)
        if (nodeType[bndEdges[i]] != WALL) freeBoundary = 1;
    if (!freeBoundary) rows[nRows++] = 3 * (PetscInt) elems[0] + 2;

    TRY(VecSet(x, 0.0));                        // the imposed values
    err = MatZeroRows(A, nRows, rows, 1.0, x, b);
    free(rows);
    TRY(err);

    status->assemble_time = (double) (clock() - start) / CLOCKS_PER_SEC;

    /* ---- solve ---- */
    start = clock();
    KSPConvergedReason reason;
    PetscInt iterations;
    TRY(KSPReset(ksp));                         // the size may change at every step
    TRY(KSPSetOperators(ksp, A, A));
    TRY(KSPSolve(ksp, b, x));
    TRY(KSPGetConvergedReason(ksp, &reason));
    TRY(KSPGetIterationNumber(ksp, &iterations));
    status->solve_time = (double) (clock() - start) / CLOCKS_PER_SEC;
    status->ksp_iterations = (int) iterations;

    // |A x - b| / |b|
    Vec r;
    PetscReal rNorm, bNorm;
    TRY(VecDuplicate(b, &r));
    TRY(MatMult(A, x, r));
    TRY(VecAXPY(r, -1.0, b));
    TRY(VecNorm(r, NORM_2, &rNorm));
    TRY(VecNorm(b, NORM_2, &bNorm));
    status->residual_norm = (double) (bNorm > 0 ? rNorm / bNorm : rNorm);

    if (reason > 0) {
        const PetscScalar* xa;
        TRY(VecGetArrayRead(x, &xa));
        for (PetscInt i = 0; i < n; i++) uvp[i] = (double) xa[i];
        TRY(VecRestoreArrayRead(x, &xa));
        status->success = 1;
    } else {
        fprintf(stderr, "[solver] the linear solver failed (%s), uvp is "
                        "unchanged\n", KSPConvergedReasons[reason]);
    }

    TRY(VecDestroy(&r));
    TRY(VecDestroy(&x));
    TRY(VecDestroy(&b));
    TRY(MatDestroy(&A));
    return 0;
}

solver_status_st solve_navier_stokes(
    solver_st* solver,
    double   const * nodes, uint32_t nnodes,
    uint32_t const * elems, uint32_t nelems,
    node_type_et const * node_type,
    uint32_t const * bnd_edges, uint32_t n_bnd_edges,
    physical_props_st const * props, double* uvp) {

    solver_status_st status = {0};
    status.n_dofs = 3 * (int) nnodes;

    if (solver == NULL || solver->ksp == NULL) {
        fprintf(stderr, "[solver] the solver was not created (solver_create)\n");
        return status;
    }

    int* nTriAtNode = malloc((size_t) nnodes * sizeof(int) + 1);
    if (checkInput(nodes, nnodes, elems, nelems, bnd_edges, n_bnd_edges,
                   props, nTriAtNode)) {
        if (assembleAndSolve(solver->ksp, nodes, nnodes, elems, nelems,
                             node_type, bnd_edges, n_bnd_edges, props,
                             nTriAtNode, uvp, &status)) {
            fprintf(stderr, "[solver] PETSc error, see the message above\n");
            status.success = 0;
        }
    }
    free(nTriAtNode);
    return status;
}

#endif // HAVE_PETSC
