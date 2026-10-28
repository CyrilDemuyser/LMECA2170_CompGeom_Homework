// Half-edge gets for mesh_st Task 1 / section 1.1 / next/twins/origins/starts/shared triangles/neighbor triangles/triangle vertices
#include "He_st.h"

int get_he_next(int he) {
    return (he / 3) * 3 + (he + 1) % 3;
}

int get_he_twin(mesh_st const * mesh, int he) {
    return mesh->twin[he];
}

uint32_t get_he_origin(mesh_st const * mesh, int he) {
    return mesh->elems[he];
}

uint32_t get_he_start(mesh_st const * mesh, uint32_t v) {
    return mesh->startHE[v];
}

int get_shared_triangle(mesh_st const * mesh, int he) {
    int twin_he = get_he_twin(mesh, he);
    if (twin_he == -1) { // Boundary half-edge, no shared triangle
        return -1;
    }
    return twin_he / 3; // Return the index of the shared triangle
}

void get_neighbor_triangles(mesh_st const * mesh, uint32_t t, uint32_t* neighbors) {
    for (int i = 0; i < 3; i++){
        uint32_t he = 3* t + i;
        int twin_he = get_he_twin(mesh, he);
        if (twin_he != -1) {
            neighbors[i] = twin_he / 3; // Store the index of the neighboring triangle
        }
    }
}

void get_vertices_triangle(mesh_st const * mesh, uint32_t t, uint32_t* vertices) {
    vertices[0] = mesh->elems[3*t];
    vertices[1] = mesh->elems[3*t + 1];
    vertices[2] = mesh->elems[3*t + 2];
}
