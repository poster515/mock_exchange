import ctypes
import os


class ArchivePublication:
    def __init__(self):
        self.archive_lib = ctypes.CDLL('/usr/local/lib/libarchive_shared.dylib')
        self.archive_lib.archive_pub_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        self.archive_lib.archive_pub_create.restype = ctypes.POINTER(ctypes.c_void_p)

        self.archive_lib.archive_pub_close.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        self.archive_lib.archive_pub_close.restype = None

        self.archive_lib.archive_pub_destroy.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        self.archive_lib.archive_pub_destroy.restype = None

        self.archive_lib.archive_pub_claim.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t]
        self.archive_lib.archive_pub_claim.restype = ctypes.POINTER(ctypes.c_uint8)

        self.archive_lib.archive_pub_commit.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        self.archive_lib.archive_pub_commit.restype = ctypes.c_size_t

        self.publication_handle = None

        print(self.archive_lib.archive_pub_create)
        print(self.archive_lib.archive_pub_close)
        print(self.archive_lib.archive_pub_destroy)

    def __del__(self):
        self.publication_close()

    def publication_open(self, shm_name: str, shm_size: int):
        # first close any open publication we may already have
        self.publication_close()

        file_name = os.path.join("tmp", shm_name)
        self.publication_handle = self.archive_lib.archive_pub_create(file_name.encode("utf-8"), shm_size)
        print(f"python: got new handle {self.publication_handle}")

    def publication_status(self) -> bool:
        if self.publication_handle == None:
            return None

        return self.archive_lib.archive_pub_is_ready(self.publication_handle)
    
    def publication_close(self):
        if self.publication_handle is None:
            return

        self.archive_lib.archive_pub_close(self.publication_handle)
        self.archive_lib.archive_pub_destroy(self.publication_handle)

    def publication_claim(self, size: int, format_func):
        # these are cumulative - you can claim any number of spots here and then commit them later
        format_func(self.archive_lib.archive_pub_claim(size))

    def publication_commit(self) -> int:
        return self.archive_lib.archive_pub_commit()
    

if __name__ == '__main__':
    archive = ArchivePublication()
    archive.publication_open("archive_test", 1024)

    # tries = 0
    # while not archive.publication_status() and tries < 10:
    #     time.sleep(2)
    #     tries += 1

    # if archive.publication_status():
    #     print("Archive opened!")

    # else:
    #     print("Archive not ready still")

    print("[python] attempting to close publication")
    archive.publication_close()