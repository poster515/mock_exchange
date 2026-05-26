#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "archive/ArchivePublication.h"

class ArchivePublicationTest : public ::testing::Test {
protected:
    static constexpr const char* SHM_NAME = "/ipc_queue_test";
    static constexpr size_t QUEUE_SIZE = 4096;
    static constexpr size_t SMALL_QUEUE_SIZE = 128;

    void SetUp() override {
        // Initialize any necessary test fixtures
    }

    void TearDown() override {
        // Cleanup after tests
    }
};

TEST_F(ArchivePublicationTest, PublicationCreation) {
    archive::ArchivePublication publication {
        archive::ArchivePublication::ArchivePublicationParams {
            .queue_params {
                .file_name = SHM_NAME,
                .queue_size = QUEUE_SIZE,
                .is_writer = true
            }
        }
    };
    EXPECT_FALSE(publication.is_ready());

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    message_transport::IpcQueue reader(
        message_transport::IpcQueue::IpcQueueParameters{
            .file_name = SHM_NAME,
            .queue_size = QUEUE_SIZE,
            .is_writer = false
        }
    );
    EXPECT_TRUE(publication.is_ready());

}

TEST_F(ArchivePublicationTest, ClaimBuffer) {
    archive::ArchivePublication publication{
        archive::ArchivePublication::ArchivePublicationParams {
            .queue_params {
                .file_name = SHM_NAME,
                .queue_size = QUEUE_SIZE,
                .is_writer = true
            }
        }
    };
    auto buffer = publication.claim_buffer(1024);
    EXPECT_GE(buffer.size_bytes(), 1024);
}

TEST_F(ArchivePublicationTest, CommitBuffer) {
    archive::ArchivePublication publication{
        archive::ArchivePublication::ArchivePublicationParams {
            .queue_params {
                .file_name = SHM_NAME,
                .queue_size = QUEUE_SIZE,
                .is_writer = true
            }
        }
    };
    auto buffer = publication.claim_buffer(512);
    
    memcpy(buffer.data(), "test", 4);
    EXPECT_EQ(publication.commit(), 1);
}

TEST_F(ArchivePublicationTest, ClaimAndCommitMultiple) {
    archive::ArchivePublication publication{
        archive::ArchivePublication::ArchivePublicationParams {
            .queue_params {
                .file_name = SHM_NAME,
                .queue_size = QUEUE_SIZE,
                .is_writer = true
            }
        }
    };
    
    for (int i = 0; i < 5; ++i) {
        auto buffer = publication.claim_buffer(256);
    }
    
    EXPECT_EQ(5, publication.uncommitted_message_count());
    EXPECT_EQ(5, publication.commit());
}

TEST_F(ArchivePublicationTest, BufferCapacity) {
    archive::ArchivePublication publication{
        archive::ArchivePublication::ArchivePublicationParams {
            .queue_params {
                .file_name = SHM_NAME,
                .queue_size = QUEUE_SIZE,
                .is_writer = true
            }
        }
    };
    size_t requestedSize = 2048;
    auto buffer = publication.claim_buffer(requestedSize);
    
    EXPECT_GE(buffer.size_bytes(), requestedSize);
}

// TEST_F(ArchivePublicationTest, WaitForArchiveHealth) {
//     archive::ArchivePublication publication;
//     bool healthy = publication.waitForArchiveHealth(5000);
//     EXPECT_TRUE(healthy);
// }

TEST_F(ArchivePublicationTest, PublicationClose) {
    archive::ArchivePublication publication{
        archive::ArchivePublication::ArchivePublicationParams {
            .queue_params {
                .file_name = SHM_NAME,
                .queue_size = QUEUE_SIZE,
                .is_writer = true
            }
        }
    };
    EXPECT_FALSE(publication.is_ready());
}
