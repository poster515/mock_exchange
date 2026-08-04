
import os


from alpha.shared.base_agent import BaseAgent
from alpha.shared.archive_constants import ArchiveConstants


class ArchiveSubscription:

    def __init__(self, shm_name: str, agent: BaseAgent, shm_size = ArchiveConstants.DEFAULT_QUEUE_SIZE):
        self.subscription_handle = None
        self.shm_name = shm_name
        self.owning_agent: BaseAgent = agent
        self.shm_size = shm_size

        self.subscription_open(self.shm_size)

    def __del__(self):
        self.subscription_close()
    
    def subscription_open(self, shm_size: int):
        # first close any open publication we may already have
        self.subscription_close()

        agent_name = self.owning_agent.name
        file_name = os.path.join(ArchiveConstants.DEFAULT_SHM_PATH, self.shm_name)
        self.subscription_handle = ArchiveConstants.archive_lib.archive_sub_create(file_name.encode("utf-8"), shm_size, agent_name.encode("utf-8"))
        print(f"python: got new subscription handle {self.subscription_handle} at file '{file_name}' for '{agent_name}'")

    def subscription_status(self) -> bool:
        if self.subscription_handle == None:
            return None

        return ArchiveConstants.archive_lib.archive_sub_is_ready(self.subscription_handle)
    
    def subscription_close(self):
        if self.subscription_handle is None:
            return

        ArchiveConstants.archive_lib.archive_sub_close(self.subscription_handle)
        ArchiveConstants.archive_lib.archive_sub_destroy(self.subscription_handle)
        self.subscription_handle = None

    def poll_subscription(self, callback):
        if self.subscription_handle is None:
            return

        ArchiveConstants.archive_lib.archive_sub_poll(self.subscription_handle, callback)