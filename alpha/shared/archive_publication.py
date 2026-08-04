
import os
from typing import Any

from alpha.shared.base_agent import BaseAgent
from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_buffer import ClaimedBuffer

class ArchivePublication:

    def __init__(self, shm_name: str, agent: BaseAgent, shm_size = ArchiveConstants.DEFAULT_QUEUE_SIZE):
        self.publication_handle = None
        self.shm_name = shm_name
        self.owning_agent: BaseAgent = agent
        self.shm_size = shm_size

        self.publication_open(self.shm_size)

    def __del__(self):
        self.publication_close()

    def publication_open(self, shm_size: int):
        # first close any open publication we may already have
        self.publication_close()

        agent_name = self.owning_agent.name
        file_name = os.path.join(ArchiveConstants.DEFAULT_SHM_PATH, self.shm_name)
        self.publication_handle = ArchiveConstants.archive_lib.archive_pub_create(file_name.encode("utf-8"), shm_size, agent_name.encode("utf-8"))
        print(f"python: got new publication handle {self.publication_handle} at file '{file_name}' for '{agent_name}'")

    def publication_status(self) -> bool:
        if self.publication_handle == None:
            return None

        return ArchiveConstants.archive_lib.archive_pub_is_ready(self.publication_handle)
    
    def publication_close(self):
        if self.publication_handle is None:
            return

        ArchiveConstants.archive_lib.archive_pub_close(self.publication_handle)
        ArchiveConstants.archive_lib.archive_pub_destroy(self.publication_handle)
        self.publication_handle = None

    def publication_claim(self, message_type: Any) -> ClaimedBuffer:
        # these are cumulative - you can claim any number of spots here and then commit them later
        # EXCEPT when using these in a with...as loop. ClaimedBuffer will auto-commit upon exiting.
        return ClaimedBuffer(self.publication_handle, message_type)

    def publication_commit(self) -> int:
        return ArchiveConstants.archive_lib.archive_pub_commit(self.publication_handle)
