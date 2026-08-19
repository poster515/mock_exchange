
import time
import os

from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_messages import NewSymbolAdd, AgentShutdown, AllAgentsShutdown

from alpha.agents.agent import AlpacaAgent

class DummyAgent(AlpacaAgent):
    def __init__(self, name):
        super().__init__(name)
        self.is_live: bool = True
        self.shutdown_reason: str = ""

    # required by BaseAgent
    def handle_admin_bytes(self, bytes, size):
        pass

    # required by BaseAgent
    def handle_shutdown(self, reason):
        print(f"{self._name} got shutdown signal for {reason}")
        self.is_live = False
        self.shutdown_reason = reason

    # required by AlpacaAgent
    def execute_strategy(self, epoch_sec: float) -> None:
        super().execute_strategy(epoch_sec)

def test_symbol_add():
    print("------------------- SYMBOL ADD --------------------")

    dummy2 = DummyAgent("dummy2")
    dummy2.start()

    creation_time_sec = time.time()

    print("Attempting to write data to publication...")

    admin_pub1: ArchivePublication = ArchivePublication(ArchiveConstants.ADMIN_QUEUE, "dummy_admin_controller")
    with admin_pub1.publication_claim(NewSymbolAdd) as new_symbol:
        new_symbol.symbolName = b"AAPL"
        new_symbol.symbolId = 1234
        new_symbol.createTimeEpochNs = int(creation_time_sec) * 1000000

    dummy2.execute_strategy(time.time())

    print(f"Got the following keys: {dummy2.admin_client.symbol_mapping}")
    assert("AAPL" in dummy2.admin_client.symbol_mapping.keys())
    assert(dummy2.admin_client.symbol_mapping["AAPL"] == 1234)


def test_shutdown_signal():
    print("------------------- ADMIN SHUTDOWN --------------------")
    dummy1 = DummyAgent("dummy1")
    dummy2 = DummyAgent("dummy2")
    queue_name = os.path.join(ArchiveConstants.DEFAULT_SHM_PATH, ArchiveConstants.ADMIN_QUEUE)
    ArchiveConstants.archive_lib.archive_force_close(queue_name.encode("utf-8"))

    dummy1.start()
    dummy2.start()

    print("Attempting to write data to publication...")
    admin_pub1: ArchivePublication = ArchivePublication(ArchiveConstants.ADMIN_QUEUE, "dummy_admin_controller")
    with admin_pub1.publication_claim(AgentShutdown) as shutdown:
        shutdown.destAgentName = b"dummy1000"
        shutdown.reason = b"INVALID_AGENT"

    dummy2.execute_strategy(time.time())

    assert(dummy2.is_live)
    assert(dummy2.shutdown_reason == "")

    with admin_pub1.publication_claim(AgentShutdown) as shutdown:
        shutdown.destAgentName = b"dummy2"
        shutdown.reason = b"INVALID_AGENT"

    dummy2.execute_strategy(time.time())
    assert(not dummy2.is_live)
    assert(dummy2.shutdown_reason == "INVALID_AGENT")



def test_shutdown_all_signal():
    print("------------------- ADMIN SHUTDOWN --------------------")
    dummy1 = DummyAgent("dummy1")
    dummy2 = DummyAgent("dummy2")
    queue_name = os.path.join(ArchiveConstants.DEFAULT_SHM_PATH, ArchiveConstants.ADMIN_QUEUE)
    ArchiveConstants.archive_lib.archive_force_close(queue_name.encode("utf-8"))

    dummy1.start()
    dummy2.start()

    print("Attempting to write data to publication...")
    admin_pub1: ArchivePublication = ArchivePublication(ArchiveConstants.ADMIN_QUEUE, "dummy_admin_controller")
    with admin_pub1.publication_claim(AllAgentsShutdown) as shutdown:
        shutdown.reason = b"INVALID_AGENT"

    dummy2.execute_strategy(time.time())
    assert(not dummy2.is_live)
    assert(dummy2.shutdown_reason == "INVALID_AGENT")
