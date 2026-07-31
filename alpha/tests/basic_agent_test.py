import ctypes

from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription
from alpha.shared.archive_messages import NewOrderSingle, TimeInForce, OrderSide, MessageHeader, OrderType
from alpha.shared.archive_constants import MessageCallback

QUEUE_SIZE = 4096

def test_hello():
    print("inside python test")
    assert(True)

def test_archive_pub():
    print("inside archive pub test")
    ArchiveConstants.archive_lib.archive_force_close("/test_archive".encode("utf-8"))

    archive = ArchivePublication("test_archive", "archive_pub_test")
    archive.publication_open(QUEUE_SIZE)

    with archive.publication_claim(NewOrderSingle) as order:
        order.orderId = 1234
        order.symbol = b"AAPL"
        order.side = OrderSide.Buy
        order.orderQty = 10
        order.price = 100
        order.priceFactor = 100000

def test_archive_sub():
    print("inside archive sub test")
    ArchiveConstants.archive_lib.archive_force_close("/test_archive".encode("utf-8"))
    archive = ArchiveSubscription("test_archive", "archive_sub_test")
    archive.subscription_open(QUEUE_SIZE)

def test_archive_pub_and_sub():
    print("inside pub and sub test")
    ArchiveConstants.archive_lib.archive_force_close("/test_archive".encode("utf-8"))

    writer = ArchivePublication("test_archive", "archive_pub_sub_test_writer")
    writer.publication_open(QUEUE_SIZE)
    reader = ArchiveSubscription("test_archive", "archive_pub_sub_test_reader")
    reader.subscription_open(QUEUE_SIZE)

    with writer.publication_claim(NewOrderSingle) as order:
        order.orderId = 1234
        order.symbol = b"AAPL"
        order.side = OrderSide.Buy
        order.orderQty = 10
        order.ordType = OrderType.Market
        order.price = 100
        order.priceFactor = 100000
        order.timeInForce = TimeInForce.Day

    def callback(bytes, size) -> int:
        print(f"Received {size} bytes from poll: {bytes}")
        assert(size >= ctypes.sizeof(MessageHeader))

        header = ctypes.cast(bytes, ctypes.POINTER(MessageHeader)).contents
        assert(header.blockLength == ctypes.sizeof(NewOrderSingle) - ctypes.sizeof(MessageHeader))
        assert(header.templateId == NewOrderSingle.TEMPLATE_ID)
        assert(header.schemaId == 1)
        assert(header.version == 1)

        full_message = ctypes.cast(bytes, ctypes.POINTER(NewOrderSingle)).contents
        assert(full_message.orderId == 1234)
        assert(full_message.symbol == b"AAPL")
        assert(full_message.side == 1)
        assert(full_message.orderQty == 10)
        assert(full_message.ordType == 1)
        assert(full_message.price == 100)
        assert(full_message.priceFactor == 100000)
        assert(full_message.timeInForce == 0)
        return 0

    cb = MessageCallback(callback)
    reader.poll_subscription(cb)