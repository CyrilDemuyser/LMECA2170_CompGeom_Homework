/*
 *   Some C functions to highlight the use of Python bindings
 *   
 *   compile with :
 *      (Linux/WSL)   gcc -shared -fPIC -o libbindings.so     ./src.c
 *      (Mac)         gcc -shared -fPIC -o libbindings.dylib  ./src.c
 *      (Windows)     gcc -shared -fPIC -o libbindings.dll    ./src.c
 *
 *   For windows, if you don't have GCC then you should compile the code 
 *   as a SHARED LIBRARY in your IDE. I don't use either an IDE nor windows 
 *   so I don't know much more.
 *
 *   To check out compilation with CMake, you can look at the CMakeLists.txt in this directory.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// ================================ Simple demo ===============================
// Calling a C function from python 
// ============================================================================

void hello_from_C(void) {
    printf("Printing hello from C !\n");
}



// ========================= Modify inputs from numpy =========================
// Taking numpy arrays as input and modifying them inplace 
// ============================================================================

// Take two arrays of size n and compute a = a + b
void add(double * const restrict a, double const * const restrict b, uint32_t const n){ 
    for (uint32_t i = 0; i < n; ++i){
        a[i] += b[i];
    }
}



// ======================= Returning C memory to python =======================
// Allocating memory from C and then giving it to python 
// WARNING: memory allocated from C has to be freed in C too !
//          (can be done automatically by encapsulating the functions in 
//          a python class overloading the __del__ function : see test.py)
// ============================================================================

double* allocate_data(int size, double fill_value) {
    double* data = malloc(sizeof(*data) * size);
    for (int i = 0; i < size; ++i){
        data[i] = fill_value;
    }
    return data;
}

void free_data(void* data) {
    free(data);
}


// ========================= Interfacing structs ==============================
// Allows to easily exchange structs between C and python. E.g. for simulation 
// parameters
// ============================================================================

typedef struct mystruct a_nice_struct;
struct mystruct {
    char* msg;
    int option;
    float param1;
    float param2;
};

void function_taking_struct(a_nice_struct* s) {
    printf(
        "message: %s\n"
        "option: %d\n"
        "param1: %e\n"
        "param2: %e\n",
        s->msg, s->option, s->param1, s->param2
    );

    // Overwrite s.option
    s->msg = "But this is cooler!";
}
