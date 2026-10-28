#include "student_spatial_search.h"
#include <float.h>
#include <stdint.h>
#include <stdlib.h>

/* Function documentation is in student_spatial_search.h
 *
 * Some inline functions of interest are defined there
 */


// ================================= BBox ==================================
// Inverted bounding box so that it is extended by everything
bbox_st bbox_empty(void) {
    return (bbox_st){
        .min = {DBL_MAX, DBL_MAX},
        .max = {-DBL_MAX, -DBL_MAX},
    };
}

int bbox_contains_node(bbox_st const* bb, double const * node) {
    // todo
    return 0;
}

int bbox_intersect_edge(bbox_st const * bb, double const * node, uint32_t s, uint32_t t) {
    // todo
    return 0;
}

bbox_st edge_get_bbox(double const* nodes, uint32_t s, uint32_t t) {
    // todo
    return bbox_empty();
}

bbox_st bbox_merge(bbox_st const a, bbox_st const b) {
    // todo
    return bbox_empty();
}


void bbox_add_edge(bbox_st * bb, double const * node, uint32_t s, uint32_t t) {
    // todo
    return;
}

// ================================ Quadtree ================================

quadtree_st* quadtree_new(uint32_t bucket_size, uint32_t max_depth) {
    quadtree_st* const qt = calloc(1, sizeof(*qt));
    qt->bucket_size = bucket_size;
    qt->max_depth = max_depth;
    return qt;
}

void quadtree_destroy(quadtree_st* qtree) {
    // DO NOT free nodes/edges. These should remain usable after quadtree deletion 
    // (as they were before its construction)
    qnode_st * qnodes = qtree->qnodes;
    for (uint32_t i = 0; i < qtree->nqnodes; ++i){
        if (qnodes[i].objects) {
            free(qnodes[i].objects);
            qnodes[i].objects = NULL;
        }
    }
    free(qtree->qnodes);
    qtree->qnodes = NULL;
}


void quadtree_set_edges(quadtree_st * qt, double const * nodes, uint32_t const * edges, uint32_t nnodes, uint32_t nedges) {
    // compute bounding box, initialize root, pre-allocate qnodes...
    return;
}

void quadtree_build(quadtree_st* qt) {
    // compute the hierarchy/bounding boxes ...
}

neirest_edge_st quadtree_find_closest_edge(quadtree_st const* qt, double const px, double const py) {
    return (neirest_edge_st) {0};
}

intersection_st quadtree_find_intersecting_edge(quadtree_st const* qt, double const * nodes, uint32_t s, uint32_t t) {
    return (intersection_st){0};
}
