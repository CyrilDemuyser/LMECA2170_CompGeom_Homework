// Getter functions for the main : returns the he-data struct has raw arrays of triangles, nodes etc
#ifndef HE_GET_H
#define HE_GET_H

#include "HeObject.h"

/*
 * All the getters hand back a READ-ONLY view straight into the mesh: no copy,
 * no malloc, nothing to free. They stay valid until the next operation that
 * adds a node or a triangle, because that one may realloc the arrays.
 *
 *     int n;
 *     const double* xy = getNodes(mesh, &n);
 *     for (int v = 0; v < n; v++) printf("%f %f\n", xy[2*v], xy[2*v+1]);
 *
 * Every `n...` out-parameter may be NULL if you do not need the count.
 */

// Node coordinates, 2*nNodes doubles: {x0,y0, x1,y1, ...}
const double* getNodes(const Mesh* mesh, int* nNodes);

// Triangle connectivity, 3*nTriangles node indices: {t0a,t0b,t0c, t1a, ...}
// The triangles owns the half-edges, i.e. 3t, 3t+1 and 3t+2,
// triangles[3t+i] is the i-th node of triangle t and the origin of he 3t+i
const int* getTriangles(const Mesh* mesh, int* nTriangles);

// (For later) Size field, one double per node.
const double* getSizefield(const Mesh* mesh, int* nNodes);

// (For later) Node type, one int per node.
const int* getNodeType(const Mesh* mesh, int* nNodes);

// (For later) Node velocities, 2*nNodes doubles.
const double* getNodeVel(const Mesh* mesh, int* nNodes);

/* --- half-edge level, to walk on the structure --- */

// One outgoing half-edge per node (HE_BOUNDARY if the node is isolated).
const int* getStartHE(const Mesh* mesh, int* nNodes);

// The twin array, 3*nTriangles entries.
const int* getHeTwin(const Mesh* mesh, int* nHalfEdges);

int getNumNodes(const Mesh* mesh);
int getNumTriangles(const Mesh* mesh);


#endif
