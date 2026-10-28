// (Some) Geometric operations on the Half-Edge data structure :-)
#ifndef HE_OPP_H
#define HE_OPP_H

#include "HeObject.h"

/*
 * The three operations never move a half-edge from one triangle to another:
 * they only rewrite triangles[] and twin[]. The invariant "triangle t owns the
 * half-edges 3t, 3t+1, 3t+2" therefore holds for free.
 *
 * All three keep startHE[] up to date for the nodes they disturb.
 */

/*
 * Swaps an edge between two elements.
 *
 *        v2                    v2
 *        /|\                   / \
 *       / | \                 /   \
 *      /  |  \               /     \
 *    v0   |   v1     -->   v0-------v1
 *      \  |  /               \     /
 *       \ | /                 \   /
 *        \|/                   \ /
 *        v3                    v3
 *      (he = v0->v1)         (now v2->v3)
 *
 * `he` is any half-edge of the edge to swap. Returns:
 *     0  swap done
 *    -1  `he` is on the boundary, there is nothing to swap with
 *    -2  the swap was refused: the two triangles do not form a convex quad,
 *        so swapping would create a flat or an inverted triangle.
 */
int edgeSwap(Mesh* mesh, int he);

/*
 * Splits an edge by adding a point on it -> creates two new faces.
 *
 *        v2                      v2
 *        /|\                     /|\
 *       / | \                   / | \
 *      /  |  \                 /  |  \
 *    v0   |   v1     -->     v0---n---v1      n = new node (x,y)
 *      \  |  /                 \  |  /
 *       \ | /                   \ | /
 *        \|/                     \|/
 *        v3                      v3
 *
 * Two new triangles on an interior edge, one on a boundary edge.
 * (x,y) is expected to lie on the edge -- the midpoint is the usual choice.
 * Returns the index of the new node, or -1 if `he` is invalid.
 */
int edgeSplit(Mesh* mesh, int he, double x, double y);

/*
 * Splits a face by adding a point on it -> creates two new faces.
 *
 *        v2                      v2
 *        / \                     /|\
 *       /   \                   / | \
 *      /     \        -->      /  n  \          n = new node (x,y)
 *     /       \               / /   \ \
 *    /         \             //       \\
 *  v0-----------v1          v0---------v1
 *
 * (x,y) is expected to lie inside triangle `tri` -- the centroid is the usual
 * choice. Returns the index of the new node, or -1 if `tri` is invalid.
 */
int faceSplit(Mesh* mesh, int tri, double x, double y);


#endif // !HE_OPP_H
