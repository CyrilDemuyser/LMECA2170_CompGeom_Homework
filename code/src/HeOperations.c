#include "HeOperations.h"

#include <float.h>
#include <math.h>

/*
 * Does (a,b,c) turn strictly counter-clockwise?
 *
 * The answer is the sign of the determinant below, but that determinant is
 * computed in floating point: when the three nodes are nearly (or exactly)
 * aligned, the rounding error is as big as the result itself and its sign
 * means nothing. So we only answer "yes" when the value is larger than its
 * own error bound, and say "no" when we cannot tell. Refusing a swap is
 * always safe; accepting a wrong one flattens a triangle for good.
 * (Deciding the undecided cases needs exact predicates, cf. Shewchuk.)
 */
static int isStrictlyCCW(const Mesh* mesh, int a, int b, int c)
{
    const double* p = mesh->coords;

    double d1 = (p[2*b + 0] - p[2*a + 0]) * (p[2*c + 1] - p[2*a + 1]);
    double d2 = (p[2*b + 1] - p[2*a + 1]) * (p[2*c + 0] - p[2*a + 0]);

    double det   = d1 - d2;
    double bound = 8.0 * DBL_EPSILON * (fabs(d1) + fabs(d2));

    return det > bound;
}


/* ========================================================================== */
/*  edgeSwap                                                                  */
/* ========================================================================== */
/*
 * Naming used below (a = he, the edge to swap is v0->v1) :
 *
 *              v2                          v2
 *              /\                          /|\
 *         c   /  \   b                  c / | \ b            a : v2->v0
 *            /    \                      /  |  \             b : v0->v3
 *           /  a   \                    /   |   \            c : v3->v2
 *         v0------->v1        -->     v0    |a d| v1         d : v2->v3
 *           \  d   /                    \   |   /            e : v3->v1
 *            \    /                      \  |  /             f : v1->v2
 *         e   \  /   f                  e \ | / f
 *              \/                          \|/
 *              v3                          v3
 *
 * The 6 half-edges stay in their own triangle, we only rewrite where they
 * start and who their twin is.
 */
int edgeSwap(Mesh* mesh, int he)
{
    int a = he;
    int d = mesh->twin[a];
    if (d == HE_BOUNDARY) return -1;            // nothing on the other side

    int b = heNext(a), c = heNext(b);
    int e = heNext(d), f = heNext(e);

    int v0 = mesh->triangles[a];
    int v1 = mesh->triangles[b];
    int v2 = mesh->triangles[c];
    int v3 = mesh->triangles[f];

    // The swapped edge is v2->v3: it must cut the quad into two CCW triangles,
    // which is exactly the quad being convex.
    if (!isStrictlyCCW(mesh, v2, v0, v3) || !isStrictlyCCW(mesh, v2, v3, v1))
        return -2;

    // read the outside twins before overwriting anything
    int tb = mesh->twin[b], tc = mesh->twin[c];
    int te = mesh->twin[e], tf = mesh->twin[f];

    // triangle of a becomes (v2,v0,v3), triangle of d becomes (v2,v3,v1)
    mesh->triangles[a] = v2;  mesh->triangles[b] = v0;  mesh->triangles[c] = v3;
    mesh->triangles[d] = v2;  mesh->triangles[e] = v3;  mesh->triangles[f] = v1;

    meshSetTwins(mesh, a, tc);                  // a is v2->v0, c used to be
    meshSetTwins(mesh, b, te);                  // b is v0->v3, e used to be
    meshSetTwins(mesh, e, tf);                  // e is v3->v1, f used to be
    meshSetTwins(mesh, f, tb);                  // f is v1->v2, b used to be
    meshSetTwins(mesh, c, d);                   // the new interior edge

    int touched[6] = {a, b, c, d, e, f};
    for (int i = 0; i < 6; i++) meshUpdateStartHE(mesh, touched[i]);

    return 0;
}


/* ========================================================================== */
/*  edgeSplit                                                                 */
/* ========================================================================== */
/*
 *              v2                            v2
 *              /\                            /|\
 *         c   /  \   b                    c / | \ i      b : v0->n   (T1)
 *            /    \                        /  |  \       h : v1->v2  (T3)
 *           /  a   \                      / T1|T3 \      g : n->v1   (T3)
 *         v0------->v1        -->       v0----n----v1
 *           \  d   /                      \ T2|T4 /      d : n->v0   (T2)
 *            \    /                        \  |  /       j : v1->n   (T4)
 *         e   \  /   f                    e \ | / l      l : v3->v1  (T4)
 *              \/                            \|/
 *              v3                            v3
 *
 * T1 (a,b,c) becomes (v0,n,v2) and T2 (d,e,f) becomes (n,v0,v3);
 * T3 (g,h,i) = (n,v1,v2) and T4 (j,k,l) = (v1,n,v3) are appended.
 */
int edgeSplit(Mesh* mesh, int he, double x, double y)
{
    if (he < 0 || he >= 3 * mesh->nTriangles) return -1;

    int a = he;
    int b = heNext(a), c = heNext(b);
    int d = mesh->twin[a];                      // -1 on the boundary

    int v1 = mesh->triangles[b];            // triangles[a] stays v0, it never moves
    int v2 = mesh->triangles[c];
    int tb = mesh->twin[b];

    int n = meshAddNode(mesh, x, y);

    /* ---- side of `a` : T1 keeps (v0,n,v2), T3 takes (n,v1,v2) ---- */
    mesh->triangles[b] = n;                     // a: v0->n, b: n->v2, c: v2->v0

    int t3 = meshAddTriangle(mesh);
    int g = 3*t3, h = g + 1, i = g + 2;
    meshSetTriangleNodes(mesh, t3, n, v1, v2);

    meshSetTwins(mesh, h, tb);                  // h is v1->v2, b used to be
    meshSetTwins(mesh, i, b);                   // new interior edge n-v2
    meshSetTwins(mesh, g, HE_BOUNDARY);         // fixed below if interior

    if (d == HE_BOUNDARY) {
        meshSetTwins(mesh, a, HE_BOUNDARY);     // v0->n stays on the boundary

        int touched[6] = {a, b, c, g, h, i};
        for (int k = 0; k < 6; k++) meshUpdateStartHE(mesh, touched[k]);
        return n;
    }

    /* ---- side of `d` : T2 keeps (n,v0,v3), T4 takes (v1,n,v3) ---- */
    int e = heNext(d), f = heNext(e);
    int v3 = mesh->triangles[f];
    int tf = mesh->twin[f];

    mesh->triangles[d] = n;                     // d: n->v0, e: v0->v3, f: v3->n

    int t4 = meshAddTriangle(mesh);
    int j = 3*t4, k = j + 1, l = j + 2;
    meshSetTriangleNodes(mesh, t4, v1, n, v3);

    meshSetTwins(mesh, l, tf);                  // l is v3->v1, f used to be
    meshSetTwins(mesh, k, f);                   // new interior edge n-v3
    meshSetTwins(mesh, j, g);                   // the split edge, v1 side
    meshSetTwins(mesh, a, d);                   // the split edge, v0 side
                                                //

    int touched[12] = {a, b, c, d, e, f, g, h, i, j, k, l};
    for (int m = 0; m < 12; m++) meshUpdateStartHE(mesh, touched[m]);

    return n;
}


/* ========================================================================== */
/*  faceSplit                                                                 */
/* ========================================================================== */
/*
 *              v2                          v2
 *              /\                          /|\
 *         c   /  \   b                  g / |i\ e          T  (a,b,c) = (v0,v1,n)
 *            /    \                      / T3|T2\          T2 (d,e,f) = (v1,v2,n)
 *           /      \          -->       /   c|f  \         T3 (g,h,i) = (v2,v0,n)
 *          /   a    \                  /  h  n  b \
 *        v0----------v1              v0-----------v1
 *                                          a
 */
int faceSplit(Mesh* mesh, int tri, double x, double y)
{
    if (tri < 0 || tri >= mesh->nTriangles) return -1;

    int a = 3*tri, b = a + 1, c = a + 2;        // the triangle owns 3t,3t+1,3t+2

    int v0 = mesh->triangles[a];
    int v1 = mesh->triangles[b];
    int v2 = mesh->triangles[c];
    int tb = mesh->twin[b];                     // twin[a] never changes
    int tc = mesh->twin[c];

    int n = meshAddNode(mesh, x, y);

    mesh->triangles[c] = n;                     // T becomes (v0,v1,n)

    int t2 = meshAddTriangle(mesh);
    int d = 3*t2, e = d + 1, f = d + 2;
    meshSetTriangleNodes(mesh, t2, v1, v2, n);

    int t3 = meshAddTriangle(mesh);
    int g = 3*t3, h = g + 1, i = g + 2;
    meshSetTriangleNodes(mesh, t3, v2, v0, n);

    meshSetTwins(mesh, d, tb);                  // d is v1->v2, b used to be
    meshSetTwins(mesh, g, tc);                  // g is v2->v0, c used to be

    meshSetTwins(mesh, b, f);                   // the three new interior edges
    meshSetTwins(mesh, c, h);
    meshSetTwins(mesh, e, i);

    int touched[9] = {a, b, c, d, e, f, g, h, i};
    for (int k = 0; k < 9; k++) meshUpdateStartHE(mesh, touched[k]);

    return n;
}
