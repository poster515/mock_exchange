
from alpha.shared.archive_constants import ArchiveConstants
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription
from alpha.shared.archive_messages import NewOrderSingle, OrderType, OrderSide
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
        order.price = 100
        order.priceFactor = 100000

    def callback(bytes, len) -> int:
        print(f"Received {len} bytes from poll")
        return 0

    cb = MessageCallback(callback)
    reader.poll_subscription(cb)