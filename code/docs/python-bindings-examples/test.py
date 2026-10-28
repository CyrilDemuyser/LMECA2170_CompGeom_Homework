import numpy as np 
import ctypes
from numpy import ctypeslib as npc
import pathlib

# You first have to load the compiled C library into Python
libnames = list(pathlib.Path(".").glob("libbindings.*"))
if (not len(libnames)):
    raise FileExistsError("The shared library file does not exists (or is in the wrong place)")

# Load dynamically the library
lib = npc.load_library("libbindings", ".")

# =============================================================================
# Write the bindings. This lets python know how to call C functions
# Every function should have a restype (the return type) and a list 
# describing every type of arguments
# =============================================================================

hello_from_C = lib.hello_from_C
hello_from_C.restype = None
hello_from_C.argtypes = None


# ============================ Using numpy arrays ============================
add = lib.add 
add.restype = None
add.argtypes = [
    npc.ndpointer(dtype=np.float64, ndim=1, flags=("C_CONTIGUOUS", "WRITEABLE")),
    npc.ndpointer(dtype=np.float64, ndim=1, flags=("C_CONTIGUOUS")),
    ctypes.c_uint32
]


# ============================= Managing C memory =============================

allocate_data = lib.allocate_data
allocate_data.restype = ctypes.POINTER(ctypes.c_double)
allocate_data.argtypes = [
    ctypes.c_int,
    ctypes.c_double
]

free_data = lib.free_data 
free_data.restype = None 
free_data.argtypes = [
    ctypes.c_void_p
]

# you can write simple python classes to handle memory management automagically
class MemManager:
    def __init__(self, size, fill_val):
        # You can add additional parameter checks here, but I'm lazy 
        # (yes this code has been written by hand by the assistant, no AI required!)

        self._data = allocate_data(size, fill_val)
        self.arr = npc.as_array(self._data, (size,))

    def __del__(self):
        # This function is automatically called whe the object is destructed by 
        # Python. This way, you don't have to call manually the freeing function ! 
        print("Look : I'm automatically destroyed !")
        free_data(self._data)

# ========================== Interfacing structs =============================
# Define the C struct
class MyStruct(ctypes.Structure):
    _fields_ = [
        ("msg", ctypes.c_char_p),  
        ("option", ctypes.c_int),  
        ("param1", ctypes.c_float),
        ("param2", ctypes.c_float) 
    ]

function_taking_struct = lib.function_taking_struct
function_taking_struct.restype = None 
function_taking_struct.argtypes = [
    ctypes.POINTER(MyStruct)
]


if __name__ == "__main__":

    # Simple demo
    hello_from_C()


    # Modify inputs from numpy
    a = np.full(10, 1.0)
    b = np.full(10, 2.0)
    add(a, b, 10)
    print(f"a+b = {a}\n")


    # Allocate data in C the low-level way
    data = allocate_data(10, 42.0)   # ===> This is a raw pointer
    print(f"allocate_data allocates raw pointers : {data}")
    print(f"But you can read from it : {data[0]}, {data[1]}, {data[2]}, {data[3]}, {data[4]}...\n")

    np_data = npc.as_array(data, (10,)) # ===> This is called a "view", it can read/modify the data but 
                                        #      it does not own the memory, you have to free it yourself.
    print(f"And you can transform it into a numpy array : {np_data}\n")

    free_data(data)  # ===> free only when you're done !

    # Allocate data in C the higher level way 
    data = MemManager(10, 42.0)
    print(f"Writing a memory manager is easy : {data.arr}\n")


    # Interfacing structs
    struct = MyStruct(b"This is a cool struct", 42, 1.67, 3.14)
    function_taking_struct(ctypes.byref(struct))   # ==> byref sends a pointer : default is a copy !
    print(f"struct.msg after C call: {struct.msg}\n")



