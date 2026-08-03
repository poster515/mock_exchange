
from abc import ABC, abstractmethod

class BaseAgent(ABC):

    def __init__(self, name: str):
        self._name = name

    @property
    def name(self):
        return self._name

    @abstractmethod
    def handle_admin_bytes(bytes, size):
        pass