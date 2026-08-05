from ctypes import *

from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_messages import MessageHeader

class ClaimedBuffer:

    def __init__(self, pub, message_type):
        self._pub = pub
        self._message_type = message_type

        self._ptr = None
        self._msg = None

    def __enter__(self):

        size = sizeof(self._message_type)
        # print(f"Attempting to claim {size} bytes from queue...")
        self._ptr = ArchiveConstants.archive_lib.archive_pub_claim(self._pub, size)

        self._msg = cast(
            self._ptr,
            POINTER(self._message_type)
        ).contents

        self._msg.header.templateId = self._message_type.TEMPLATE_ID
        self._msg.header.schemaId = 1
        self._msg.header.version = 1
        self._msg.header.blockLength = sizeof(self._message_type) - sizeof(MessageHeader)

        return self._msg

    def __exit__(self, exc_type, exc, tb):

        if exc_type is None:
            ArchiveConstants.archive_lib.archive_pub_commit(self._pub)
        else:
            # TODO: we should add an abort() call here - should be as simple as adding an abort/no-op enum type in queue
            pass

        # Returning False propagates any exception.
        return False