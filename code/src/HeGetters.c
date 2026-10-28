#include "HeGetters.h"

#include <stddef.h>

/*
 * Nothing but views. The `const` on the return type is what keeps the caller
 * (the main, a plotting library, ...) from breaking the invariants behind the
 * back of the operations.
 */

static void setCount(int* out, int value)
{
    if (out != NULL) *out = value;
}

const double* getNodes(const Mesh* mesh, int* nNodes)
{
    setCount(nNodes, mesh->nNodes);
    return mesh->coords;
}

const int* getTriangles(const Mesh* mesh, int* nTriangles)
{
    setCount(nTriangles, mesh->nTriangles);
    return mesh->triangles;
}

const double* getSizefield(const Mesh* mesh, int* nNodes)
{
    setCount(nNodes, mesh->nNodes);
    return mesh->sizefield;
}

const int* getNodeType(const Mesh* mesh, int* nNodes)
{
    setCount(nNodes, mesh->nNodes);
    return mesh->nodeType;
}

const double* getNodeVel(const Mesh* mesh, int* nNodes)
{
    setCount(nNodes, mesh->nNodes);
    return mesh->nodeVel;
}

const int* getStartHE(const Mesh* mesh, int* nNodes)
{
    setCount(nNodes, mesh->nNodes);
    return mesh->startHE;
}

const int* getHeTwin(const Mesh* mesh, int* nHalfEdges)
{
    setCount(nHalfEdges, 3 * mesh->nTriangles);
    return mesh->twin;
}

int getNumNodes(const Mesh* mesh)     { return mesh->nNodes; }
int getNumTriangles(const Mesh* mesh) { return mesh->nTriangles; }
