
import os
from alpha.shared.archive_constants import ArchiveConstants


class ArchiveSubscription:

    def __init__(self, shm_name: str):
        self.subscription_handle = None
        self.shm_name = shm_name

    def __del__(self):
        self.subscription_close()
    
    def subscription_open(self, shm_size: int):
        # first close any open publication we may already have
        self.subscription_close()

        file_name = os.path.join(ArchiveConstants.DEFAULT_SHM_PATH, self.shm_name)
        self.subscription_handle = ArchiveConstants.archive_lib.archive_sub_create(file_name.encode("utf-8"), shm_size)
        print(f"python: got new handle {self.subscription_handle}")

    def subscription_status(self) -> bool:
        if self.subscription_handle == None:
            return None

        return ArchiveConstants.archive_lib.archive_sub_is_ready(self.subscription_handle)
    
    def subscription_close(self):
        if self.subscription_handle is None:
            return

        ArchiveConstants.archive_lib.archive_sub_close(self.subscription_handle)
        ArchiveConstants.archive_lib.archive_sub_destroy(self.subscription_handle)

    def poll_subscription(self, callback):
        if self.subscription_handle is None:
            return

        ArchiveConstants.archive_lib.archive_sub_poll(self.subscription_handle, callback)