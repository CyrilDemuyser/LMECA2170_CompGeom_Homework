#ifndef __HE_ST_H__
#define __HE_ST_H__

#include <stdint.h>
#include "student_triangulation.h"

int get_he_next(int he);

int get_he_twin(mesh_st const * mesh, int he);

uint32_t get_he_origin(mesh_st const * mesh, int he);

uint32_t get_he_start(mesh_st const * mesh, uint32_t v);

int get_shared_triangle(mesh_st const * mesh, int he);

void get_neighbor_triangles(mesh_st const * mesh, uint32_t t, uint32_t* neighbors);

void get_vertices_triangle(mesh_st const * mesh, uint32_t t, uint32_t* vertices);



#endif // __HE_ST_H__