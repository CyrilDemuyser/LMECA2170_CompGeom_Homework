#ifndef __STUDENT_TRIANGULATION_H__
#define __STUDENT_TRIANGULATION_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =========================== Structures ============================

// Minimal boundary structure 
typedef struct {
    // You can modify anything you need. e.g. tags for wall contact...
    double * nodes;  // Coordinates of the boundary nodes in row-major. 
                     // NOTE: this can contains nodes that are not in the boundary, 
                     //       as long as the @edges contains only boundary nodes
    uint32_t * edges; // The boundary edges
    uint32_t nnodes; // number of nodes
    uint32_t nedges; // number of edges
} boundary_st;

// Minimal mesh structure
typedef struct {
    // You can modify/add anything you need. e.g. half-edges, boundary, node type, ...
    double * nodes;      // Coordinates of the mesh nodes in row-major
    double * size_field; // Target size of the mesh at each node. Size=nnodes
    uint32_t * elems;    // Triangles of the mesh
    uint32_t nnodes;     // number of nodes
    uint32_t nelems;     // number of elems

    // Half-edges
    // elems[he] is the origin vertex; next is computed, not stored.
    uint32_t * twin;   // half-edge opposite of current

    uint32_t * startHE; // half-edge leaving vertex (!takes a vertex index, returns a half-edge index)
} mesh_st;

// ============================= Functions ============================

/* Triangulate the nodes of an existing triangulation. Tries to reuse memory if it exists.
 *
 * @param mesh [in/out]: on input, the mesh object to use. The triangulation will use the mesh.nodes 
 *                       on output, the mesh.elems contains the Delaunay triangulation
 *
 * @return mesh: The mesh constructed from @mesh.nodes
 */
void mesh_triangulate(mesh_st* mesh);

/* Retrieve the boundary of a mesh.
 *
 * @param mesh [in]: the mesh
 *
 * @return bnd: The boundary the input mesh
 */
boundary_st mesh_get_boundary(mesh_st const * mesh);

/* Delete selected elements in the mesh.
 *
 * We encourage you to store the half-edges inside the mesh and to modify them 
 * at the same time as deleting elements. This will make mesh adaptation 
 * a lot easier.
 *
 * @param mesh [in/out]: On input, the mesh from which to delete elements.
 *                       On output, the mesh without the deleted elements. (updated elems/nelems)
 * @param to_delete [in]: The elements to delete in the mesh. 
 *                        If to_delete[i] = 0, the i-th element is kept.
 *                        if to_delete[i] = 1, the i-th element is deleted.
 *                        Size = nelems
 *
 * @return nelems_out: the remaining element in the mesh (i.e. the number of zeros in @to_delete)
 */
uint32_t mesh_delete_elements(mesh_st* mesh, uint8_t const * to_delete);

/* Adapt a mesh according to mesh.size_field (e.g. Chew's algorithm)
 *
 * NOTE: fields is a double pointer (**) because you will need to resize (and thus reallocate)
 *       them. Take a look at "realloc(...)" : https://koor.fr/C/cstdlib/realloc.wp
 *       E.g: *fields = realloc(*fields, nfields*new_size*sizeof(double));
 *
 *       If we gave a single pointer (*), any reallocation would only 
 *       be seen from inside the function and not from the caller. Meaning that 
 *       the caller would get a freed @field afterward without explanation.
 *
 *       If you think about it, the same is true for nodes, elems, and size_field
 *       because they are pointers inside a struct pointer. Hence resizing 
 *
 *
 * @param mesh [in/out]: On input, the initial mesh with its target size field. 
 *                                 mesh.nodes contains the initial nodes 
 *                                 mesh.elements contains the initial elements 
 *                                 mesh.size_field contains the target size field at each node
 *                       On output, the adapted mesh
 *                                 mesh.nodes contains the initial nodes extended by the final nodes
 *                                 mesh.elements contains the elements of the adapted mesh
 *                                 mesh.size_field contains the linearly interpolated size field at each nodes
 * @param fields [in/out]: On input, the scalar fields at each node of the initial mesh
 *                         On output, the interpolated scalar field at each node of the final mesh
 *                         initial size = nfields*nnodes, final size = nfields* (nnodes + nadd)
 * @param nfields [in]: The number of fields to interpolate.
 *
 * @return nadd: The number of added nodes in the mesh
 */
uint32_t mesh_adapt(mesh_st* mesh, double** fields, uint32_t const nfields);

#ifdef __cplusplus
};
#endif

#endif // __STUDENT_TRIANGULATION_H__
