// Implementation of an Half-Edge data structure with indexing arrays
#ifndef HE_OBJ
#define HE_OBJ

/*
 * Nodes :  {coords, nodeId, He, nodeType, sizeField, nodeVelocity}
 * Elem  :  {elemId, He}
 * HalfE :  {triangles, twin}
 *
 * ---------------------------------------------------------------------------
 * CONVENTIONS: to read :-)
 * ---------------------------------------------------------------------------
 *
 * 1) Triangle `t` OWNS the three half-edges 3*t, 3*t+1, 3*t+2, in that order:
 *
 *              triangles[3t+2]
 *                   x
 *                  / \
 *       he 3t+2   /   \   he 3t+1
 *                /     \
 *               x-------x
 *   triangles[3t]  he 3t  triangles[3t+1]
 *
 *    Consequences:
 *      - heNext(3t+i) == 3t + (i+1)%3        (computed, never stored)
 *      - the triangle of half-edge h is      h/3
 *      - `triangles` IS the triangle connectivity array: triangles[3t+i] is the
 *         i-th node of triangle t, and the origin of half-edge 3t+i.
 *
 * 2) Half-edges turn COUNTER-CLOCKWISE around their triangle.
 *
 * 3) twin[h] == HE_BOUNDARY (-1) means h is on the boundary of the mesh.
 *
 * 4) startHE[v] is one half-edge whose origin is v. If v sits on the boundary,
 *    we always store the outgoing half-edge that is itself on the boundary, so
 *    that turning around v with twin/heNext visits the whole one-ring.
 *
 * ---------------------------------------------------------------------------
 * MEMORY
 * ---------------------------------------------------------------------------
 * nNodes / nTriangles are the numbers of objects actually used, allocNodes /
 * allocTriangles are the numbers we have room for. Adding one more object than
 * we have room for doubles the capacity (see meshReserveNodes/Triangles).
 */

#define HE_BOUNDARY (-1)

typedef struct Mesh Mesh;

struct Mesh{

    int nNodes, nTriangles;             // objects in use
    int allocNodes, allocTriangles;     // objects we have room for

    /*
     * Every arrays that correspond to the nodes objects
     */
    int* startHE;       // Index of an he starting from the node (allocNodes)
    int* nodeType;      // (For later) Type of the node (allocNodes)
    double* coords;     // coordinates of the nodes (2*allocNodes)
    double* nodeVel;    // (For later) Velocity of the node (2*allocNodes)
    double* sizefield;  // (For later) Size field at the node (allocNodes)

    /*
     * Triangles object : they are only defined by their three half-edges,
     * 3t, 3t+1 and 3t+2, so there is nothing to store per triangle.
     *
     * Every arrays to describe the half-edges objects (3*allocTriangles)
     */
    int* triangles;     // Idx of the node that is the he origin (3*allocTriangles)
    int* twin;          // Idx of the twin he, belongs to a neighbour elem (3*allocTriangles)
};

static inline int heNext(int he) { return 3*(he/3) + (he + 1)%3; }


/* --- allocation ---------------------------------------------------------- */

// Empty mesh with room for nNodes nodes (and the triangles they can carry).
Mesh* allocateMesh(int nNodes);
void freeMesh(Mesh* mesh);

// Make sure at least n objects fit; capacity doubles until it is big enough.
void meshReserveNodes(Mesh* mesh, int n);
void meshReserveTriangles(Mesh* mesh, int n);


/* --- initialisation ------------------------------------------------------ */

// Single triangle (0,0) (1,0) (0,1).
Mesh* meshInitDefault(void);

// Single triangle given by 3 nodes, coords = {x0,y0, x1,y1, x2,y2} (CCW).
Mesh* meshInitFromTriangle(const double* coords);

// From a list of nodes (2*nNodes doubles) and a list of triangles
// (3*nTriangles node indices, each triangle CCW). Twins are rebuilt here.
Mesh* meshInitFromArrays(const double* coords, int nNodes,
                         const int* triangles, int nTriangles);


/* --- low level building blocks (used by the operations) ------------------ */

// Appends a node, returns its index.
int meshAddNode(Mesh* mesh, double x, double y);

// Appends an empty triangle (3 fresh half-edges, twins unset),
// returns its index. Its half-edges are 3*t, 3*t+1, 3*t+2.
int meshAddTriangle(Mesh* mesh);

// triangles[3t..3t+2] = v0,v1,v2 (does NOT touch the twins).
void meshSetTriangleNodes(Mesh* mesh, int t, int v0, int v1, int v2);

// Glues two half-edges together, -1 (HE_BOUNDARY) is accepted on either side.
void meshSetTwins(Mesh* mesh, int he0, int he1);

// startHE[triangles[he]] = he, keeping convention 4) above.
void meshUpdateStartHE(Mesh* mesh, int he);

// Checks every invariant, prints what is broken. Returns the number of errors.
int meshCheck(const Mesh* mesh);


#endif // !HE_OBJ
