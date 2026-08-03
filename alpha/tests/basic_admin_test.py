
import time

from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription
from alpha.shared.archive_messages import NewSymbolAdd

from alpha.agents.agent import AlpacaAgent

class DummyAgent(AlpacaAgent):
    def __init__(self, name):
        super().__init__(name)

    # required by BaseAgent
    def handle_admin_bytes(bytes, size):
        pass

    # required by AlpacaAgent
    def execute_strategy(self, epoch_sec: float) -> None:
        super().execute_strategy(epoch_sec)

# def test_symbol_add():
#     print("------------------- SYMBOL ADD --------------------")
#     dummy1 = DummyAgent("dummy1")
#     dummy2 = DummyAgent("dummy2")

#     dummy1.start()
#     dummy2.start()

#     creation_time = time.time()

#     admin_pub1: ArchivePublication = dummy1.admin_client.admin_publication
#     with admin_pub1.publication_claim(NewSymbolAdd) as new_symbol:
#         new_symbol.symbolName = b"AAPL"
#         new_symbol.symbolId = 1234
#         new_symbol.createTimeEpochNs = creation_time


#     dummy2.execute_strategy(time.time())

#     assert("AAPL" in dummy2.admin_client().symbol_mapping().keys())
#     assert(dummy2.admin_client().symbol_mapping().keys()["AAPL"] == 1234)
