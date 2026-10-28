#include "HeObject.h"

#include <stdio.h>
#include <stdlib.h>

/* ========================================================================== */
/*  Small helpers                                                             */
/* ========================================================================== */

static void* xrealloc(void* ptr, size_t size)
{
    void* p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "[HeObject] out of memory (%zu bytes)\n", size);
        exit(EXIT_FAILURE);
    }
    return p;
}

// Smallest power-of-two multiple of `cap` that reaches `needed`.
static int grow(int cap, int needed)
{
    if (cap < 1) cap = 1;
    while (cap < needed) cap *= 2;
    return cap;
}


/* ========================================================================== */
/*  Allocation                                                                */
/* ========================================================================== */

void meshReserveNodes(Mesh* mesh, int n)
{
    if (n <= mesh->allocNodes) return;

    int cap = grow(mesh->allocNodes, n);

    mesh->coords    = xrealloc(mesh->coords,    2 * (size_t)cap * sizeof(double));
    mesh->startHE   = xrealloc(mesh->startHE,       (size_t)cap * sizeof(int));
    mesh->nodeType  = xrealloc(mesh->nodeType,      (size_t)cap * sizeof(int));
    mesh->nodeVel   = xrealloc(mesh->nodeVel,   2 * (size_t)cap * sizeof(double));
    mesh->sizefield = xrealloc(mesh->sizefield,     (size_t)cap * sizeof(double));

    mesh->allocNodes = cap;
}

void meshReserveTriangles(Mesh* mesh, int n)
{
    if (n <= mesh->allocTriangles) return;

    int cap = grow(mesh->allocTriangles, n);

    mesh->triangles = xrealloc(mesh->triangles, 3 * (size_t)cap * sizeof(int));
    mesh->twin     = xrealloc(mesh->twin,      3 * (size_t)cap * sizeof(int));

    mesh->allocTriangles = cap;
}

Mesh* allocateMesh(int nNodes)
{
    Mesh* mesh = xrealloc(NULL, sizeof(Mesh));

    mesh->nNodes = 0;
    mesh->nTriangles = 0;
    mesh->allocNodes = 0;
    mesh->allocTriangles = 0;

    mesh->coords = NULL;
    mesh->startHE = NULL;
    mesh->nodeType = NULL;
    mesh->nodeVel = NULL;
    mesh->sizefield = NULL;
    mesh->triangles = NULL;
    mesh->twin = NULL;

    if (nNodes < 1) nNodes = 1;
    meshReserveNodes(mesh, nNodes);
    // Euler: a triangulation of n nodes has at most 2n-5 triangles, 2n is a
    // safe and simple first guess (it grows by itself anyway).
    meshReserveTriangles(mesh, 2 * nNodes);

    return mesh;
}

void freeMesh(Mesh* mesh)
{
    if (mesh == NULL) return;

    free(mesh->coords);
    free(mesh->startHE);
    free(mesh->nodeType);
    free(mesh->nodeVel);
    free(mesh->sizefield);
    free(mesh->triangles);
    free(mesh->twin);
    free(mesh);
}


/* ========================================================================== */
/*  Low level building blocks                                                 */
/* ========================================================================== */

int meshAddNode(Mesh* mesh, double x, double y)
{
    meshReserveNodes(mesh, mesh->nNodes + 1);   // <- the realloc happens here

    int v = mesh->nNodes++;

    mesh->coords[2*v + 0] = x;
    mesh->coords[2*v + 1] = y;
    mesh->startHE[v]      = HE_BOUNDARY;        // no half-edge yet
    mesh->nodeType[v]     = 0;
    mesh->nodeVel[2*v + 0] = 0.0;
    mesh->nodeVel[2*v + 1] = 0.0;
    mesh->sizefield[v]     = 1.0;

    return v;
}

int meshAddTriangle(Mesh* mesh)
{
    meshReserveTriangles(mesh, mesh->nTriangles + 1);

    int t = mesh->nTriangles++;
    int h = 3 * t;

    for (int i = 0; i < 3; i++) {
        mesh->triangles[h + i] = -1;
        mesh->twin[h + i]      = HE_BOUNDARY;
    }
    return t;
}

void meshSetTriangleNodes(Mesh* mesh, int t, int v0, int v1, int v2)
{
    mesh->triangles[3*t + 0] = v0;
    mesh->triangles[3*t + 1] = v1;
    mesh->triangles[3*t + 2] = v2;
}

void meshSetTwins(Mesh* mesh, int he0, int he1)
{
    if (he0 != HE_BOUNDARY) mesh->twin[he0] = he1;
    if (he1 != HE_BOUNDARY) mesh->twin[he1] = he0;
}

void meshUpdateStartHE(Mesh* mesh, int he)
{
    int v = mesh->triangles[he];
    if (v < 0) return;

    int cur = mesh->startHE[v];

    // Nothing usable stored yet, or what is stored no longer starts at v.
    if (cur == HE_BOUNDARY || mesh->triangles[cur] != v) {
        mesh->startHE[v] = he;
        return;
    }
    // Convention 4) : a boundary half-edge always wins.
    if (mesh->twin[he] == HE_BOUNDARY) mesh->startHE[v] = he;
}

// Recomputes startHE for every node from scratch (used by the initialisers).
static void meshRebuildStartHE(Mesh* mesh)
{
    for (int v = 0; v < mesh->nNodes; v++)
        mesh->startHE[v] = HE_BOUNDARY;

    for (int h = 0; h < 3 * mesh->nTriangles; h++)
        meshUpdateStartHE(mesh, h);
}


/* ========================================================================== */
/*  Rebuilding the twins from a triangle list                                 */
/* ========================================================================== */
/*
 * Two half-edges are twins when they join the same two nodes. We therefore
 * tag every half-edge with the sorted pair (min,max) of its two endpoints,
 * sort the tags, and glue the pairs that end up side by side.
 * O(nHe log nHe) instead of the naive O(nHe^2) double loop.
 */

typedef struct { int v0, v1, he; } EdgeKey;

static int compareEdgeKeys(const void* pa, const void* pb)
{
    const EdgeKey* a = (const EdgeKey*) pa;
    const EdgeKey* b = (const EdgeKey*) pb;

    if (a->v0 != b->v0) return (a->v0 < b->v0) ? -1 : 1;
    if (a->v1 != b->v1) return (a->v1 < b->v1) ? -1 : 1;
    return 0;
}

static void meshBuildTwins(Mesh* mesh)
{
    int nHe = 3 * mesh->nTriangles;
    if (nHe == 0) return;

    EdgeKey* keys = xrealloc(NULL, (size_t)nHe * sizeof(EdgeKey));

    for (int h = 0; h < nHe; h++) {
        int a = mesh->triangles[h];
        int b = mesh->triangles[heNext(h)];

        keys[h].v0 = (a < b) ? a : b;
        keys[h].v1 = (a < b) ? b : a;
        keys[h].he = h;

        mesh->twin[h] = HE_BOUNDARY;            // boundary until proven twin
    }

    qsort(keys, (size_t)nHe, sizeof(EdgeKey), compareEdgeKeys);

    int i = 0;
    while (i < nHe) {
        int j = i + 1;
        while (j < nHe && compareEdgeKeys(&keys[i], &keys[j]) == 0) j++;

        int count = j - i;
        if (count == 2) {
            meshSetTwins(mesh, keys[i].he, keys[i + 1].he);
        } else if (count > 2) {
            fprintf(stderr,
                    "[HeObject] warning: edge (%d,%d) is shared by %d triangles,"
                    " the mesh is not manifold\n", keys[i].v0, keys[i].v1, count);
        }
        i = j;
    }

    free(keys);
}


/* ========================================================================== */
/*  Initialisation                                                            */
/* ========================================================================== */

Mesh* meshInitFromTriangle(const double* coords)
{
    Mesh* mesh = allocateMesh(3);

    for (int i = 0; i < 3; i++)
        meshAddNode(mesh, coords[2*i + 0], coords[2*i + 1]);

    int t = meshAddTriangle(mesh);
    meshSetTriangleNodes(mesh, t, 0, 1, 2);
    // The three half-edges stay on the boundary (twin = -1), as set by
    // meshAddTriangle.
    meshRebuildStartHE(mesh);

    return mesh;
}

Mesh* meshInitDefault(void)
{
    static const double coords[6] = {0.0, 0.0,
                                     1.0, 0.0,
                                     0.0, 1.0};
    return meshInitFromTriangle(coords);
}

Mesh* meshInitFromArrays(const double* coords, int nNodes,
                         const int* triangles, int nTriangles)
{
    Mesh* mesh = allocateMesh(nNodes);
    meshReserveTriangles(mesh, nTriangles);

    for (int v = 0; v < nNodes; v++)
        meshAddNode(mesh, coords[2*v + 0], coords[2*v + 1]);

    for (int t = 0; t < nTriangles; t++) {
        int tt = meshAddTriangle(mesh);
        meshSetTriangleNodes(mesh, tt, triangles[3*t + 0],
                                       triangles[3*t + 1],
                                       triangles[3*t + 2]);
    }

    meshBuildTwins(mesh);
    meshRebuildStartHE(mesh);

    return mesh;
}


/* ========================================================================== */
/*  Consistency check                                                         */
/* ========================================================================== */

int meshCheck(const Mesh* mesh)
{
    int nHe = 3 * mesh->nTriangles;
    int errors = 0;

    #define HE_ERR(...) do { fprintf(stderr, "[meshCheck] " __VA_ARGS__); \
                             errors++; } while (0)

    for (int h = 0; h < nHe; h++) {

        int v = mesh->triangles[h];
        if (v < 0 || v >= mesh->nNodes)
            HE_ERR("he %d has an invalid origin (%d)\n", h, v);

        int tw = mesh->twin[h];
        if (tw == HE_BOUNDARY) continue;

        if (tw < 0 || tw >= nHe)
            { HE_ERR("he %d has an invalid twin (%d)\n", h, tw); continue; }
        if (mesh->twin[tw] != h)
            HE_ERR("twin is not symmetric: twin[%d]=%d but twin[%d]=%d\n",
                   h, tw, tw, mesh->twin[tw]);
        if (tw == h)
            HE_ERR("he %d is its own twin\n", h);

        // twins walk the same edge, backwards
        if (mesh->triangles[tw] != mesh->triangles[heNext(h)] ||
            mesh->triangles[heNext(tw)] != mesh->triangles[h])
            HE_ERR("he %d and its twin %d do not share the same edge\n", h, tw);
    }

    for (int v = 0; v < mesh->nNodes; v++) {
        int h = mesh->startHE[v];
        if (h == HE_BOUNDARY) continue;         // isolated node, allowed
        if (h < 0 || h >= nHe)
            { HE_ERR("node %d has an invalid startHE (%d)\n", v, h); continue; }
        if (mesh->triangles[h] != v)
            HE_ERR("startHE[%d]=%d but that he starts at node %d\n",
                   v, h, mesh->triangles[h]);
    }

    #undef HE_ERR

    return errors;
}
