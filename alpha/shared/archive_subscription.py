import ctypes
import os

MessageCallback = ctypes.CFUNCTYPE(
    ctypes.c_int,                       # return type
    ctypes.POINTER(ctypes.c_ubyte),     # uint8_t*
    ctypes.c_size_t                     # size_t
)

class ArchiveSubscription:
    def __init__(self):
        self.archive_lib = ctypes.CDLL('/usr/local/lib/libarchive_shared.dylib')
        self.archive_lib.archive_sub_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        self.archive_lib.archive_sub_create.restype = ctypes.POINTER(ctypes.c_void_p)

        self.archive_lib.archive_sub_close.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        self.archive_lib.archive_sub_close.restype = None

        self.archive_lib.archive_sub_is_ready.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        self.archive_lib.archive_sub_is_ready.restype = None

        self.archive_lib.archive_sub_poll.argtypes = [ctypes.POINTER(ctypes.c_void_p), MessageCallback]
        self.archive_lib.archive_sub_poll.restype = None

        self.archive_lib.archive_sub_destroy.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        self.archive_lib.archive_sub_destroy.restype = None

        self.subscription_handle = None

        print(self.archive_lib.archive_pub_create)
        print(self.archive_lib.archive_sub_close)
        print(self.archive_lib.archive_sub_is_ready)
        print(self.archive_lib.archive_sub_poll)
        print(self.archive_lib.archive_sub_destroy)

    def __del__(self):
        self.subscription_close()
    
    def subscription_open(self, shm_name: str, shm_size: int):
        # first close any open publication we may already have
        self.subscription_close()

        file_name = os.path.join("tmp", shm_name)
        self.subscription_handle = self.archive_lib.archive_sub_create(file_name.encode("utf-8"), shm_size)
        print(f"python: got new handle {self.subscription_handle}")

    def subscription_status(self) -> bool:
        if self.subscription_handle == None:
            return None

        return self.archive_lib.archive_sub_is_ready(self.subscription_handle)
    
    def subscription_close(self):
        if self.subscription_handle is None:
            return

        self.archive_lib.archive_sub_close(self.subscription_handle)
        self.archive_lib.archive_sub_destroy(self.subscription_handle)