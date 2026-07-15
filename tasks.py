# run `invoke <>` to generate wrappers from this file

from invoke import task
from pathlib import Path
from cffi import FFI


@task
def build_archive_lib(context):
    print("Building CFFI Module")
    ffi = FFI()

    this_dir = Path().absolute()
    h_file_name = this_dir / "cmult.h"
    with open(h_file_name) as h_file:
        ffi.cdef(h_file.read())

    ffi.set_source(
        "cffi_example",
        # Since you're calling a fully-built library directly, no custom source
        # is necessary. You need to include the .h files, though, because behind
        # the scenes cffi generates a .c file that contains a Python-friendly
        # wrapper around each of the functions.
        '#include "cmult.h"',
        # The important thing is to include the pre-built lib in the list of
        # libraries you're linking against:
        libraries=["cmult"],
        library_dirs=[this_dir.as_posix()],
        extra_link_args=["-Wl,-rpath,."],
    )

    ffi.compile()


@task
def create_subscription(context):

    ffi = FFI()

    # Define the C function signature
    ffi.cdef("void my_function(int);")

    # Load the shared library
    C = ffi.dlopen("path_to_your_library.so")

    # Call the C++ function
    C.my_function(42)
