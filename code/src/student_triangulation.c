#include "student_triangulation.h"
#include <stdlib.h>

// Documentation can be found in student_triangulation.h

void mesh_triangulate(mesh_st * mesh) {
    // Reuse existing memory if it exists, otherwise allocate and compute
}

boundary_st mesh_get_boundary(mesh_st const * mesh) {
    // Do the thing...
    return (boundary_st){
        .nodes=mesh->nodes,
        .edges=NULL,
        .nnodes=mesh->nnodes,
        .nedges=0};
}

uint32_t mesh_delete_elements(mesh_st* mesh, uint8_t const * to_delete){
    // Do the thing...
    return mesh->nelems;
}

uint32_t mesh_adapt(mesh_st* mesh, double** fields, uint32_t const nfields){

    // Helper to access the real data.
    double * nodes = mesh->nodes;
    double * size_field = mesh->size_field;
    uint32_t* elems = mesh->elems;
    double * pfields = (fields != NULL && nfields > 0) ? *fields : NULL;
    uint32_t nnodes = mesh->nnodes;
    uint32_t nelems = mesh->nelems;

    // Do the thing...

    // Do not forget to write the pointer to the double pointer afterward
    // so that the caller can see you've reallocated data
    mesh->nodes = nodes;
    mesh->size_field = size_field;
    mesh->elems = elems;
    mesh->nnodes = nnodes;
    mesh->nelems = nelems;

    if (fields != NULL && nfields > 0) {
        *fields = pfields;
    }
    return 0;
}
