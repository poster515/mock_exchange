
import os
from alpha.shared.archive_constants import ArchiveConstants


class ArchivePublication:

    def __init__(self, shm_name: str):
        self.publication_handle = None
        self.shm_name = shm_name

    def __del__(self):
        self.publication_close()

    def publication_open(self, shm_size: int):
        # first close any open publication we may already have
        self.publication_close()

        file_name = os.path.join(ArchiveConstants.DEFAULT_SHM_PATH, self.shm_name)
        self.publication_handle = ArchiveConstants.archive_lib.archive_pub_create(file_name.encode("utf-8"), shm_size)
        print(f"python: got new handle {self.publication_handle}")

    def publication_status(self) -> bool:
        if self.publication_handle == None:
            return None

        return ArchiveConstants.archive_lib.archive_pub_is_ready(self.publication_handle)
    
    def publication_close(self):
        if self.publication_handle is None:
            return

        ArchiveConstants.archive_lib.archive_pub_close(self.publication_handle)
        ArchiveConstants.archive_lib.archive_pub_destroy(self.publication_handle)

    def publication_claim(self, size: int, format_func):
        # these are cumulative - you can claim any number of spots here and then commit them later
        format_func(ArchiveConstants.archive_lib.archive_pub_claim(self.publication_handle, size))

    def publication_commit(self) -> int:
        return ArchiveConstants.archive_lib.archive_pub_commit(self.publication_handle)
    

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