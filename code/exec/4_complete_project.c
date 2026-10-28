#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "project_solver.h"
#include "student_triangulation.h"
#include "project_display.h"

/* This is a very simple skeleton for your final project. 
 *
 * To make it easier for you to test things you can add different command line options. 
 */

void print_usage(void) {
    printf(
            "USAGE: ./exec/complete_project [--help] -T <final time> -dt <time step> -f <max fps> --initial-mesh <file> --initial-boundary <file>\n"
            "\t -T  FLOAT  The total time (higher = more winding)\n"
            "\t -dt FLOAT  The time steps\n"
            "\t -f  FLOAT  The maximum frames per second\n"
            "\t --initial-mesh      STRING  A file containing a mesh\n"
            "\t --initial-boundary  STRING  A file containing the solid boundaries\n"
    );
}


mesh_st read_mesh_from_file(char const* fname) {
    // Read an initial mesh if you want.
    // This will allow you to generate a bunch of initial meshes and not recompile 
    // every time you want to change.
    //
    // Example of simple format: 
    //
    // number_of_nodes number_of_elems 
    // number_of_nodes lines each with the x y coordinates of a node
    // number_of_elems lines each with the a b c indices of the elements
    //
    // E.g: 
    // 3 1
    // 0 0
    // 1 0
    // 1 1
    // 0 1 2
    return (mesh_st) {0};
}

boundary_st read_boundary_from_file(char const* fname) {
    // Read a solid boundary if you want.
    // This will allow you to generate a bunch of solid boundaries and not recompile 
    // every time you want to change.
    //
    // Example of simple format: 
    //
    // number_of_nodes number_of_edges 
    // number_of_nodes lines each with the x y coordinates of a node
    // number_of_edges lines each with the a b indices of the elements
    //
    // E.g: 
    // 2 1
    // 0 0
    // 1 0
    // 0 1
    return (boundary_st) {0};
}

int main(int argc, char ** argv) {

    double T = 5.0;  // The final time
    double dt = 0.05;  // A single time step
    double fps = 60.0; // Frame per second
                       
    char const* bnd_file = NULL;
    char const* mesh_file = NULL;

    int cur_arg = 1;
    while (cur_arg < argc) {

        // There is a value to read 
        if (cur_arg + 1 < argc) {
            if (strcmp("-T", argv[cur_arg]) == 0) {
                T = strtod(argv[cur_arg+1], NULL);
                cur_arg += 2;
                continue;
            }
            if (strcmp("-dt", argv[cur_arg]) == 0) {
                dt = strtod(argv[cur_arg+1], NULL);
                cur_arg += 2;
                continue;
            }
            if (strcmp("-f", argv[cur_arg]) == 0) {
                fps = strtod(argv[cur_arg+1], NULL);
                cur_arg += 2;
                continue;
            }
            if (strcmp("--initial-mesh", argv[cur_arg]) == 0) {
                mesh_file = argv[cur_arg+1];
                cur_arg += 2;
                continue;
            }
            if (strcmp("--initial-boundary", argv[cur_arg]) == 0) {
                bnd_file = argv[cur_arg+1];
                cur_arg += 2;
                continue;
            }
        }

        // Put here the flags (i.e. options without arguments)

        print_usage();
        return EXIT_FAILURE;
    }

    boundary_st solid_boundaries = {0};
    mesh_st mesh = {0};

    physical_props_st props = {
        .density = 1000.0,
        .viscosity = 1e-3,
        .gravity = {0.0, -9.81},
        .sigma = 0.072,
        .dt = dt
    };

    if (bnd_file) {
        solid_boundaries = read_boundary_from_file(bnd_file);
    } else {
        // Default boundary is a rectangle without the top edge
        //      (-1, 1)                  (1, 1)
        //         |                       |
        //         |                       |
        //      (-1, 0)------------------(1, 0)
        solid_boundaries.nnodes = 4;
        solid_boundaries.nedges = 3;
        solid_boundaries.nodes = malloc(sizeof(double) * 2 * 4);
        solid_boundaries.edges = malloc(sizeof(uint32_t) * 2 * 3);

        solid_boundaries.nodes[2*0 + 0] = -1.0; solid_boundaries.nodes[2*0 + 1] = 1.0; 
        solid_boundaries.nodes[2*1 + 0] = -1.0; solid_boundaries.nodes[2*1 + 1] = 0.0; 
        solid_boundaries.nodes[2*2 + 0] = +1.0; solid_boundaries.nodes[2*2 + 1] = 0.0; 
        solid_boundaries.nodes[2*3 + 0] = +1.0; solid_boundaries.nodes[2*3 + 1] = 1.0; 

        solid_boundaries.edges[2*0 + 0] = 0; solid_boundaries.edges[2*0 + 1] = 1;
        solid_boundaries.edges[2*1 + 0] = 1; solid_boundaries.edges[2*1 + 1] = 2;
        solid_boundaries.edges[2*2 + 0] = 2; solid_boundaries.edges[2*2 + 1] = 3;
    }

    if (mesh_file) {
        mesh = read_mesh_from_file(mesh_file);
    } else {
        // Default mesh is a disk centered at (0, 0.5)
        uint32_t const default_n = 1000;
        mesh.nodes = malloc(2*sizeof(double)*default_n);
        mesh.nnodes = default_n;

        double const phi = (1.0 + sqrt(5.0))/2.0;
        for (uint32_t i = 0; i < default_n; ++i) {
            double const theta = i * 2.0 * M_PI * (1.0 - 1.0/phi);
            double const r = sqrt((i + 0.5)/(default_n));

            mesh.nodes[2*i] = 0.2 * r * cos(theta);
            mesh.nodes[2*i+1] = 0.5 + 0.2 * r * sin(theta);
        }
    }

    solver_st* solver = solver_create();
    double* uvp = calloc(3*mesh.nnodes, sizeof(double));

    node_type_et * node_type = calloc(mesh.nnodes, sizeof(node_type_et)); // Type of nodes: BULK, FREE_SURFACE, WALL
    boundary_st fluid_boundaries = {0};  // Boundary of the fluid mesh
    fluid_boundaries.nodes = mesh.nodes;
    fluid_boundaries.nnodes = mesh.nnodes;

    for (double t = 0.0; t <= T; t += dt) {

        // Compute the mesh
        
        // Compute the alpha-shape 

        // Adapt the mesh 

        // Check which nodes are close to the solid boundaries (coloring)
        
        solver_status_st status = solve_navier_stokes(solver, mesh.nodes, mesh.nnodes, mesh.elems, mesh.nelems, node_type, fluid_boundaries.edges, fluid_boundaries.nedges, &props, uvp);

        // Constrain the displacement to the solid boundaries

        // Advect the nodes

        scene_add_nodes(mesh.nodes, mesh.nnodes);
        scene_add_mesh(mesh.nodes, mesh.nnodes, mesh.elems, mesh.nelems, uvp, 3, 2);
        scene_add_edges(solid_boundaries.nodes, solid_boundaries.nnodes, solid_boundaries.edges, solid_boundaries.nedges);
        display();
    }
    display_wait();
    
    free(mesh.nodes);
    free(mesh.elems);
    free(node_type);
    free(solid_boundaries.nodes);
    free(solid_boundaries.edges);
    free(uvp);
    
    solver_destroy(solver);
    return EXIT_SUCCESS;
}
