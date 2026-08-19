
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

    _Most_ clients will and should not be able to or want to publish to the admin channel, but for
    convenience sake its easier to give this guy a publisher AND subscriber.

    YOU MUST POLL THIS OBJECT TO PROGRESS THE READER.
    """
    def __init__(self, agent: BaseAgent):
        self.owning_agent: BaseAgent = agent
        self._symbol_mapping: Dict[str, np.uint64] = {}
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
        self._admin_publication = ArchivePublication(ArchiveConstants.ADMIN_IN_QUEUE, self.owning_agent.name)
        self._admin_subscription = ArchiveSubscription(ArchiveConstants.ADMIN_QUEUE, self.owning_agent.name)

    def teardown(self):
        if self._admin_publication:
            self._admin_publication.publication_close()
        if self._admin_subscription:
            self._admin_subscription.subscription_close()

    def poll(self):
        self._admin_subscription.poll_subscription(self._admin_callback)

    def _handle_admin_inputs(self, bytes, size) -> int:
        print(f"AdminClient received {size} bytes from poll: {bytes} for agent: {self.owning_agent.name}")
        assert(size >= ctypes.sizeof(MessageHeader))

        header = ctypes.cast(bytes, ctypes.POINTER(MessageHeader)).contents

        match header.templateId:
            case NewSymbolAdd.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(NewSymbolAdd)).contents
                print(f"Got new symbol add: {full_message}")

                symbol_name = full_message.symbolName.decode("ascii").rstrip("\x00")
                symbol_id = full_message.symbolId

                if symbol_name in self._symbol_mapping.keys() and self._symbol_mapping[symbol_name] != symbol_id:
                    print(f"{self.owning_agent.name} got conflicting symbol add, new id: {symbol_name} = {symbol_id} (old = {self.symbol_mapping[symbol_name]}")
                else:
                    print(f"{self.owning_agent.name} got new symbol add: {symbol_name} = {symbol_id}")

                self._symbol_mapping[symbol_name] = symbol_id
                return 0

            case AgentShutdown.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(AgentShutdown)).contents
                agent_name = full_message.destAgentName.decode("ascii").rstrip("\x00")
                agent_id = full_message.destAgentId
                reason = full_message.reason.decode("ascii").rstrip("\x00")

                my_agent_name = self.owning_agent.name
                if my_agent_name == agent_name:
                    print(f"[{my_agent_name}] got shutdown signal for agent: {agent_name}, reason: '{reason}'")
                    self.owning_agent.handle_shutdown(reason)
                else:
                    print(f"[{my_agent_name}] got shutdown signal for another agent: {agent_name}, ignoring")
                return 0

            case AllAgentsShutdown.TEMPLATE_ID:
                full_message = ctypes.cast(bytes, ctypes.POINTER(AllAgentsShutdown)).contents
                reason = full_message.reason.decode("ascii").rstrip("\x00")

                my_agent_name = self.owning_agent.name
                print(f"[{my_agent_name}] got all shutdown signal for reason: '{reason}'")
                self.owning_agent.handle_shutdown(reason)
                return 0

            case _:
                print(f"Unknown template ID: {header.templateId}, passing to owning_agent...")
                return self.owning_agent.handle_admin_bytes(bytes, size)