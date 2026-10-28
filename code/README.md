# LMECA2170 -- project skeleton

Starter code for the numerical geometry project.


--- 

# Disclaimers

> [!IMPORTANT] 
> The goal of this project is to build a fully functioning "Particle Finite Element Method" (PFEM) 
> implementation. We chose this topic because lots of us in the teaching team 
> finds this subject genuinely fun, and it overlaps nicely with the course material.
> However, since this **is not a numerical simulation course**, we provide 
> the functions needed to perform the simulation itself, so you can focus on 
> the geometry part. This means that if anything in the simulation code 
> breaks, **it is not your job to fix it: it is ours**. Contact us and 
> we will update this repository.


> [!IMPORTANT] 
> This repo is only meant to give you a general idea of the things 
> you are supposed to implement. You are free to modify **anything and everything**
> (including the simulation part) or even start from scratch. You can do the 
> project in C, C++ or Python (sorry for the Julia fans but we're not proficient 
> enough in it to evaluate you fairly).
>
> The repo contains a skeleton of the project in C because Nathan
> thinks you should do it in C. But he knows C can be scary so he's included a
> tutorial on how to call C functions from Python in [`docs/python-binding-example/`](./docs/python-bindings-examples/). 
> This way you can use Python for the high-level logic and still implement 
> the performance-critical parts in C !
>
> Our objective is that every group builds a PFEM simulator, but it'll be funnier 
> for you if it is reasonably efficient so that you can make cool videos :)

---

## Layout

```text 
├── CMakeLists.txt      
├── docs/               // Some documentation about the project
├── exec/               // The programs you'll run
├── include             // The include files
├── README.md
└── src/                // The source files of the implementations (your workplace)
```

## What you'll need to build

To compile this project, you'll need: 

- CMake: this is the tool used to compile C across platform, you should have already used it in previous courses. https://cmake.org/download/
- A C compiler: GCC/Clang/MSVC, ... 
- OpenGL: For the built-in visualization. Should already be included in your drivers.
- glfw: For the built-in visualization. The compilation should download and compile it from source if it is not installed on your system. If that does not work, try installing it yourself: https://www.glfw.org/download

Some platform specific guidelines: 

- **Linux**: install `cmake`, `libglfw3-dev` and `libgl1-mesa-dev` on Debian/Ubuntu.
- **macOS**: install the Xcode command line tools (`xcode-select --install`)
  and CMake (`brew install cmake glfw`).
- **Windows**: work in **WSL2**, a Linux inside Windows (the fluid solver
  needs PETSc, which does not build natively on Windows):
  1. In a PowerShell run as administrator: `wsl --install`, reboot, and
     create your Linux user when Ubuntu asks for it.
  2. In the Ubuntu terminal:
     `sudo apt update && sudo apt install build-essential cmake python3 curl pkg-config git libglfw3-dev libgl1-mesa-dev`
  3. Clone the repository **inside Linux** (e.g. in `~/`), not in `/mnt/c/...`:
     compiling on the Windows drive is very slow.
  4. Follow the Linux instructions from there. On Windows 11 (or Windows 10
     with WSL from the Microsoft Store), the viewer windows open on your
     Windows desktop. VS Code with its "WSL" extension edits the code inside
     WSL as if it were local.

Once you have everything, run the following commands to compile the code:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

If everything works as intended, you should end up with the following executables: 

```text
build/exec/0_example_viewer             # Small demo of the built-in viewer
build/exec/1_triangulation              # Helps you see/debug your triangulation
build/exec/2_ashape_adaptation          # Helps you see/debug your triangulation-alpha shape-adaptation
build/exec/3_1_distance_queries         # Helps you see/debug your distance queries
build/exec/3_2_intersection_queries     # Helps you see/debug your intersection queries
```

If you don't see those files, ask your best friend for help! Else you can run 
each of them to see what you can do with them.

## The fluid solver and PETSc

The fluid solver (`src/project_solver.c`, used in Part 3) is a compact version
of the solver of [MigFlow](https://git.immc.ucl.ac.be/fluidparticles/migflow)
(www.migflow.be, GNU LGPL v3). It solves its linear systems with
[PETSc](https://petsc.org), which you install **once**, locally, inside this
directory:

```sh
./install_petsc.sh    # ~2 to 15 minutes, needs a C compiler, make, python3 and curl
cd build
cmake ..              # should now print "PETSc found: the fluid solver is enabled"
```

The script builds PETSc in `./external/petsc` and installs nothing anywhere
else: to remove it, delete `./external`. Until PETSc is installed, CMake prints
a warning but everything else builds; only the fluid solver refuses to run.
(If you already have a PETSc of your own, `export PETSC_DIR=...` before
calling cmake, it is used instead.)

The linear solver can be changed without recompiling, through PETSc options
with the prefix `fluid_`, e.g.
`PETSC_OPTIONS="-fluid_ksp_type gmres -fluid_pc_type ilu -fluid_ksp_monitor" ./exec/4_complete_project`.
The default, a direct sparse LU, is fine for the mesh sizes of this project.

## Adding files in CMake

### For "library" files

The general idea is the following: 

- `STUDENT_PROJECT_FILES` in the `CMakeLists.txt` should contains all the `.c` files that you write.
- `STUDENT_INCLUDE_DIRS` in the `CMakeLists.txt` should contain all the directories that contains `.h` files (e.g. `include/`).

In other words:

- If you add `.h` files in `include/` you don't need to do anything. 
- If you add `.h` files in another directory, you need to add this directory to `STUDENT_INCLUDE_DIRS`.
- If you add `.c` files in `src/` you need to add these files to `STUDENT_PROJECT_FILES`.

If at some point you encounter an error that looks like this: 
```text 
/usr/bin/ld: CMakeFiles/0_example_viewer.dir/0_example_viewer.c.o: in function `showMesh':
/exec/0_example_viewer.c:213:(.text+0x450): undefined reference to `scene_add_mesh'
/usr/bin/ld: /exec/0_example_viewer.c:214:(.text+0x455): undefined reference to `display'
```
Then you forgot to add the `.c` files to `STUDENT_PROJECT_FILES`.

And if you encounter an error that looks like this:
```text
/src/HeGetters.c:1:10: fatal error: HeGetters.h: No such file or directory
    1 | #include "HeGetters.h"
      |          ^~~~~~~~~~~~~
```

Then you forgot to add a directory containing `.h` files to `STUDENT_INCLUDE_DIRS`.

### For "executable" files

You can have as many executable as you want to show different tests and example, but 
each executable must be a single file containing a `main` function. 

To add an executable `exec/name_of_executable.c`, simpy add `name_of_executable` in the `EXEC_LIST` of `exec/CMakeLists.txt`. 
It is as simple as that. 

## The project 

Take a look at the project statement for details about what you should implement. 
This skeleton is simply here to help you jumpstart your development and give you 
a general idea of how to structure the project. 

For those that want an idea of how much time they should spent on each part, here it is: 

- Triangulation: 3-4 weeks. This is the baseline of the project, it is worth it to spend a bit more 
                 time on it to fully grasp the concept.
- Alpha-Shape: 1 hour. No, really. This is a single function that can be implemented in a few lines of C. 
- Adaptation: 1-2 weeks. This will be quite similar to the triangulation work and that is why you should make sure 
              your triangulation pipeline is clean. If your node insertion mechanism is clean and you successfully 
              implemented the "node at infinity" then this should not be that hard. Do not hesitate to clean up the 
              triangulation pipeline. 
- Quadtree: 2-3 weeks. Having a basic implementation is quite straightforward, having an efficient implementation is harder.
            Take some time to make sure the distance and intersection queries are ok, if the simulation behaves weirdly at the boundary
            then most likely this is the thing that does not work correctly.
- Simulation: the remaining time. Patching all the pieces together should be fast enough. Once you get everything 
              working you can start having some fun with the physics and think of cool videos to show us. 

It is entirely possible to finish the project faster than this. If this is your case, 
we encourage you to improve your work by, e.g:

- Improving the efficiency of each step.
- Computing a mesh size based on the distance to the fluid surface (you can reuse your quadtree, but you'll have to rebuild one every iteration)
- Swapping the alpha-shape for an In/Out query based on the advected previous fluid boundary (take the boundary of your mesh, advect it and use check if the elements of the next mesh is inside this curve) this can be done with Fast Winding Numbers or Ray Casting (both uses the quadtree, but you'll need to adapt it)
- Testing different simulation cases

## Docs

See [the viewer guide](docs/viewer.md) for the data format, a minimal example,
controls, and an overview of how rendering works.

There is also an example of how to write Python bindings for C functions. 
We would really appreciate it if you do the project in C because it forces you to 
use simple arrays (and there's really not much more needed) and to understand what 
you're doing. 
But if you're really uncomfortable with C, we'd rather you do a Python version 
then re-write the performance critical functions (triangulation, quadtree searches)
in C. That way you'll be able to call the C version from Python directly.

Unfortunately, computational geometry algorithms (especially the triangulation and spatial queries)
do not vectorize well at all so you will not be able to take advantages of numpy. 

But we believe 
