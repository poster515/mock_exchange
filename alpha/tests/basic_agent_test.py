import ctypes
import time
import threading

from alpha.shared.base_agent import BaseAgent
from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription
from alpha.shared.archive_messages import NewOrderSingle, TimeInForce, OrderSide, MessageHeader, OrderType
from alpha.shared.archive_constants import MessageCallback

QUEUE_SIZE = 4096

class DummyAgent(BaseAgent):
    def __init__(self, name: str):
        super().__init__(name)

    def handle_admin_bytes(self, bytes, size):
        pass

    def handle_shutdown(self, reason):
        pass

def test_hello():
    print("inside python test")
    assert(True)

def test_archive_pub():
    print("inside archive pub test")
    dummy = DummyAgent("dummy")
    ArchiveConstants.archive_lib.archive_force_close("/test_archive".encode("utf-8"))

    archive = ArchivePublication("test_archive", dummy.name)

    with archive.publication_claim(NewOrderSingle) as order:
        order.orderId = 1234
        order.symbol = b"AAPL"
        order.side = OrderSide.Buy
        order.orderQty = 10
        order.price = 100
        order.priceFactor = 100000

def test_archive_sub():
    dummy = DummyAgent("dummy")
    print("inside archive sub test")
    ArchiveConstants.archive_lib.archive_force_close("/test_archive".encode("utf-8"))
    archive = ArchiveSubscription("test_archive", dummy.name)

def test_archive_pub_and_sub():
    print("inside pub and sub test")
    dummy = DummyAgent("dummy")
    dummy2 = DummyAgent("dummy2")
    ArchiveConstants.archive_lib.archive_force_close("/test_archive".encode("utf-8"))

    writer = ArchivePublication("test_archive", dummy.name)
    reader = ArchiveSubscription("test_archive", dummy2.name)

    with writer.publication_claim(NewOrderSingle) as order:
        order.orderId = 1234
        order.symbolId = 102030
        order.side = OrderSide.Buy
        order.orderQty = 10
        order.ordType = OrderType.Market
        order.price = 100
        order.priceFactor = 100000
        order.timeInForce = TimeInForce.Day

    def callback(bytes, size) -> int:
        # print(f"Received {size} bytes from poll: {bytes}")
        assert(size >= ctypes.sizeof(MessageHeader))

        header = ctypes.cast(bytes, ctypes.POINTER(MessageHeader)).contents
        assert(header.blockLength == ctypes.sizeof(NewOrderSingle) - ctypes.sizeof(MessageHeader))
        assert(header.templateId == NewOrderSingle.TEMPLATE_ID)
        assert(header.schemaId == 1)
        assert(header.version == 1)

        full_message = ctypes.cast(bytes, ctypes.POINTER(NewOrderSingle)).contents
        assert(full_message.orderId == 1234)
        assert(full_message.symbolId == 102030)
        assert(full_message.side == 1)
        assert(full_message.orderQty == 10)
        assert(full_message.ordType == 1)
        assert(full_message.price == 100)
        assert(full_message.priceFactor == 100000)
        assert(full_message.timeInForce == 0)
        return 0

    cb = MessageCallback(callback)
    reader.poll_subscription(cb)

def test_pub_sub_load():
    print("inside pub and sub test")
    dummy = DummyAgent("dummy")
    dummy2 = DummyAgent("dummy2")
    ArchiveConstants.archive_lib.archive_force_close("/test_archive".encode("utf-8"))

    writer = ArchivePublication("test_archive", dummy.name)
    reader = ArchiveSubscription("test_archive", dummy2.name)

    NUM_MESSAGES: int = 1E4

    def buffer_writer(w: ArchivePublication, num_messages: int = NUM_MESSAGES):
        print(f"Writer '{w.owning_agent_name}' writing {num_messages} messages to archive '{w.file_name}'")

        for i in range(0, int(num_messages)):
            with writer.publication_claim(NewOrderSingle) as order:
                order.orderId = 1234
                order.symbolId = 102030
                order.side = OrderSide.Buy
                order.orderQty = 10
                order.ordType = OrderType.Market
                order.price = i
                order.priceFactor = 100000 + i
                order.timeInForce = TimeInForce.Day

            time.sleep(0.001)

    def buffer_reader(r: ArchiveSubscription, num_messages: int = NUM_MESSAGES):
        print(f"Writer '{r.owning_agent_name}' writing {num_messages} messages to archive '{r.file_name}'")

        def reader_callback(bytes, size) -> int:
            # print(f"Received {size} bytes from poll: {bytes}, total_received: {reader_callback.num_received}")
            assert(size >= ctypes.sizeof(MessageHeader))
    
            header = ctypes.cast(bytes, ctypes.POINTER(MessageHeader)).contents
            assert(header.blockLength == ctypes.sizeof(NewOrderSingle) - ctypes.sizeof(MessageHeader))
            assert(header.templateId == NewOrderSingle.TEMPLATE_ID)
            assert(header.schemaId == 1)
            assert(header.version == 1)
    
            full_message = ctypes.cast(bytes, ctypes.POINTER(NewOrderSingle)).contents
            assert(full_message.orderId == 1234)
            assert(full_message.symbolId == 102030)
            assert(full_message.side == 1)
            assert(full_message.orderQty == 10)
            assert(full_message.ordType == 1)
            assert(full_message.price == reader_callback.num_received)
            assert(full_message.priceFactor == 100000 + reader_callback.num_received)
            assert(full_message.timeInForce == 0)

            reader_callback.num_received += 1
            return 0

        cb = MessageCallback(reader_callback)
        reader_callback.num_received = 0

        while reader_callback.num_received < num_messages - 1:
            r.poll_subscription(cb)
            time.sleep(0.001)

    write_thread = threading.Thread(target=buffer_writer, args=(writer,), kwargs={})
    read_thread = threading.Thread(target=buffer_reader, args=(reader,), kwargs={})

    write_thread.start()
    read_thread.start()

    write_thread.join()
    read_thread.join()
