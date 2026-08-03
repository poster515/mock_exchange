
from typing import Dict

import numpy as np

from alpha.shared.base_agent import BaseAgent
from alpha.shared.archive_constants import *
from alpha.shared.archive_messages import *
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription


class AdminClient:
    """
    Responsible for handling admin messages e.g., new symbol adds. This class handles some things
    but it should be given an agent that can handle other more nuanced commands.
    """
    def __init__(self, agent: BaseAgent):
        self.owning_agent: BaseAgent = agent
        self._symbol_mapping: Dict[str, np.uint64]
        self._admin_subscription = None
        self._admin_publication = None

        self._admin_callback = MessageCallback(self._handle_admin_inputs)

    def __del__(self):
        self.teardown()

    @property
    def symbol_mapping(self):
        return self._symbol_mapping

    @property
    def admin_subscription(self):
        return self._admin_subscription

    @property
    def admin_publication(self):
        return self._admin_publication

    def start(self):
        self._admin_publication = ArchivePublication(ArchiveConstants.ADMIN_OUT_QUEUE, self.owning_agent.name + "_admin_writer")
        self._admin_subscription = ArchiveSubscription(ArchiveConstants.ADMIN_IN_QUEUE, self.owning_agent.name + "_admin_reader")

    def teardown(self):
        pass

    def poll(self):
        self._admin_subscription.poll_subscription(self._admin_callback)

    def _handle_admin_inputs(self, bytes, size):
        print(f"AdminClient received {size} bytes from poll: {bytes} for agent: {self.owning_agent.name}")
        assert(size >= ctypes.sizeof(MessageHeader))

        header = ctypes.cast(bytes, ctypes.POINTER(MessageHeader)).contents
        if header.templateId == NewSymbolAdd.TEMPLATE_ID:
            full_message = ctypes.cast(bytes, ctypes.POINTER(NewSymbolAdd)).contents

            symbol_name = ctypes.create_string_buffer(full_message.symbolName, ctypes.sizeof(full_message.symbolName))
            symbol_id = full_message.symbolId

            if symbol_name in self._symbol_mapping.keys() and self._symbol_mapping[symbol_name] != symbol_id:
                print(f"{self.owning_agent.name} got conflicting symbol add, new id: {symbol_name} = {symbol_id} (old = {self.symbol_mapping[symbol_name]}")
            else:
                print(f"{self.owning_agent.name} got new symbol add: {symbol_name} = {symbol_id}")

            self._symbol_mapping[symbol_name] = symbol_id

        else:
            self.owning_agent.handle_admin_bytes(bytes, size)