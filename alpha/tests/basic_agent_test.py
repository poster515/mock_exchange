
from alpha.shared.archive_publication import ArchivePublication
from alpha.shared.archive_subscription import ArchiveSubscription
from alpha.shared.archive_messages import NewOrderSingle, OrderType, OrderSide


def test_hello():
    print("inside python test")
    assert(True)

def test_archive_pub():
    print("inside archive pub test")
    archive = ArchivePublication("test_archive")
    archive.publication_open(1024)

    # with archive.publication_claim(NewOrderSingle) as order:
    #     order.orderId = "signal.client_order_id"
    #     order.symbol = "AAPL"
    #     order.side = OrderSide.Buy
    #     order.orderQty = 10
    #     order.price = 100
    #     order.priceFactor = 100000

def test_archive_sub():
    print("inside archive sub test")
    archive = ArchivePublication("test_archive")
    archive.publication_open(1024)

def test_archive_pub_and_sub():
    print("inside pub and sub test")
    pass