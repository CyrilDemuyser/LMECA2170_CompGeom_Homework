#ifndef __STUDENT_SPATIAL_SEARCH_H__
#define __STUDENT_SPATIAL_SEARCH_H__

/* NOTE: We propose a quadtree, but if you want another spatial search structure
 *       feel free to change
 *
 * This quadtree is intended to perform the node-wall distance detection and 
 * displacement-wall intersection. Therefore it only needs to store EDGES.
 */

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================== Bounding box ===============================

// Axis Aligned Bounding Box
typedef struct {
    double min[2];  // bottom left corner
    double max[2];  // top right corner
} bbox_st;

/* Return an empty bounding box */
bbox_st bbox_empty(void);

/* Checks that a node is inside a bounding box
 *
 * @param bb [in] : the bounding box
 * @param node [in] : the list of nodes
 *
 * @return is_in: 1 if the node is inside the bounding box, 0 otherwise
 */
int bbox_contains_node(bbox_st const * bb, double const * node);

/* Checks that an edge (nodes[s], nodes[t]) intersects a bounding box
 *
 * @param bb [in]: the bounding box
 * @param node [in]: the list of nodes
 * @param s [in]: the source node index of the edge
 * @param t [in]: the target node index of the edge
 *
 * @return intersects: 1 if the edge intersect the bounding box, 0 otherwise
 */
int bbox_intersect_edge(bbox_st const * bb, double const * node, uint32_t s, uint32_t t);

/* Compute the bbox of the edge (nodes[s], nodes[t]).
 *
 * @param nodes [in]: The list of nodes 
 * @param s [in]: The source node index
 * @param t [in]: The target node index
 *
 * @return bb: the bounding box of the edge
 */
bbox_st edge_get_bbox(double const* nodes, uint32_t s, uint32_t t);

/* Return the bounding box of the bounding boxes a, b.
 *
 * @param a [in]: a bounding box
 * @param b [in]: another bounding box
 *
 * @return bb: the bounding box containing a and b
 */
bbox_st bbox_merge(bbox_st const a, bbox_st const b);


/* Enlarge the bounding box with a single edge (nodes[s], nodes[t])
 *
 * @param bb [in/out]: on input, the bounding box
 *                     on output, the enlarged bounding box
 *
 * @param node [in]: the nodes array for the edges
 * @param s [in]: the source node of the edge
 * @param t [in]: the target node of the edge
 */
void bbox_add_edge(bbox_st * bb, double const * node, uint32_t s, uint32_t t);

// ================================ Quadtree nodes ===========================

/* A node of the quadtree.
 *
 * It can either be internal or leaf. 
 * An internal quadtree node is in the tree, it has a parent and children
 * A leaf stores the indices of the edges it contains and does not have children
 */
typedef struct {
    uint32_t * objects;  // If the node is internal: NULL if it is an internal 
                         // If the node is a leaf: indices of the objects in this node 
                         //                        i.e. quadtree->edges[node->objects[i]]
    uint32_t nobjects;   // Number of objects in the leaf
    uint32_t children[4]; // indices of the children 

    bbox_st bbox;  // If the node is internal: Bounding box of the subtree
                   // If the node is leaf: Bounding box of the edge list
} qnode_st;


/* Return whether a quatree node is internal or not
 *
 * @param qnode [in]: the quadtree node
 *
 * @return is_internal: 0 if the node is not internal, 1 if it is
 */
inline int qnode_is_internal(qnode_st const* qnode) { return qnode->objects != NULL; }


/* Return whether a quatree node is a leaf or not
 * 
 * (This is the opposite of @qnode_is_internal)
 *
 * @param qnode [in]: the quadtree node
 *
 * @return is_leaf: 0 if the node is not a leaf, 1 if it is
 */
inline int qnode_is_leaf(qnode_st const* qnode) { return qnode->objects == NULL; }

// ================================ Quadtree ==================================

/* The quadtree */
typedef struct {
    // Edges in the quadtree
    double const * nodes;    // Node for the edges. 
    uint32_t const * edges;  // Edges stored in the quadtree
    uint32_t nnodes;
    uint32_t nedges;

    // Quadtree nodes stored contiguously.
    // The root is at index 0
    qnode_st* qnodes; // Quadtree nodes
    uint32_t nqnodes; // Number of quadtree nodes

    // Parameters of the quadtree. @max_depth should have higher priority than 
    // @bucket_size: if you have @bucket_size + 1 duplicate point you want them 
    // all stored on a single leaf (exceeding the suggested size) at max_depth. 
    // The alternative is an infinite recursion.
    uint32_t bucket_size; // Maximum number of objects stored in the leaves
    uint32_t max_depth;   // Maximum depth of the tree
} quadtree_st;


/* Allocate a new quatree with @bucket_size and @max_depth
 *
 * @param bucket_size [in]: the maximum number of objects stored in the leaves.
 *                          Somewhere between 4 and 32 is generally ok
 * @param max_depth [in]: the maximum depth of the quadtree. 
 *                        Should be no more than 64
 */
quadtree_st* quadtree_new(uint32_t bucket_size, uint32_t max_depth);

/* Set the edges that will be indexed in the quadtree
 *
 * @param qt [in]: the quadtree
 * @param nodes [in]: The nodes for the edges
 * @param edges [in]: the edges (as pair of node indices)
 * @param nnodes [in]: the number of nodes 
 * @param nedges [in]: the number of edges
 */
void quadtree_set_edges(quadtree_st * qt, double const * nodes, uint32_t const * edges, uint32_t nnodes, uint32_t nedges); 
 
/* Effectively build the quadtree
 *
 * @param qt [in]: the quadtree to build
 */
void quadtree_build(quadtree_st* qt);

typedef struct {
    uint32_t neirest_edge_id;  // Index of the neirest edge in the quadtree
    double distance;           // Distance between the query node and the neirest edge
    double p[2];               // Coordinate of the point on the edge that is closest to the query
} neirest_edge_st;

/* For a given node (px, py), find the closest edge in the quadtree
 *
 * @param qt [in]: the quadtree to use for the search 
 * @param px [in]: the x position of the node
 * @param py [in]: the y position of the node
 *
 * @return neirest: the info about the neirest edge (index, distance, closest point, ...)
 */
neirest_edge_st quadtree_find_closest_edge(quadtree_st const* qt, double const px, double const py);


// Represent an edge-edge intersection
typedef struct {
    int intersects;  // 1 if there is an intersection, 0 otherwise
    uint32_t intersection_edge_id; // The index of the intersecting edge in the quadtree 
    double t;  // in [0, 1], at what fraction of the edge does the intersection occur
} intersection_st;

/* For a given edge (nodes[s], nodes[t]), find the closest intersecting edge in the quadtree
 *
 * @param qt [in]: the quadtree to use for the search 
 * @param nodes [in]: the edge node array
 * @param s [in]:  the index of the edge's source node
 * @param t [in]:  the index of the edge's target node
 *
 * @return intersection: the info about the intersection (id of edge, position of intersection, ...)
 */
intersection_st quadtree_find_intersecting_edge(quadtree_st const* qt, double const * nodes, uint32_t s, uint32_t t);

/* Free the memory allocated by the quadtree. */
void quadtree_destroy(quadtree_st* qtree);

#ifdef __cplusplus
};
#endif

#endif // __STUDENT_SPATIAL_SEARCH_H__
