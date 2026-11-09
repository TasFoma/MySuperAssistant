#include "msa/core/MusicPlaylist.h"

#include <gtest/gtest.h>

#include <set>
#include <stdexcept>

namespace
{
msa::core::MusicTracks tracks()
{
    return {{"/a.mp3", "Первый"}, {"/b.wav", "Второй"}, {"/c.flac", "Третий"}};
}
TEST(MusicPlaylistTest, EmptyQueueDoesNotAdvanceOrRetry)
{
    msa::core::MusicPlaylist queue{42};
    EXPECT_EQ(queue.current(), nullptr);
    EXPECT_EQ(queue.count(), 0);
    EXPECT_EQ(queue.position(), 0);
    EXPECT_FALSE(queue.next());
    EXPECT_FALSE(queue.previous());
    EXPECT_FALSE(queue.ended());
    EXPECT_FALSE(queue.failed());
    queue.retry();
}
TEST(MusicPlaylistTest, VisitsEveryTrackAndWrapsInBothDirections)
{
    msa::core::MusicPlaylist queue{42};
    queue.replace(tracks());
    ASSERT_NE(queue.current(), nullptr);
    const auto first = *queue.current();
    std::set<std::string> visited;
    for (std::size_t index{0}; index < queue.count(); ++index)
    {
        EXPECT_EQ(queue.position(), index + 1);
        visited.insert(queue.current()->path);
        EXPECT_TRUE(queue.next());
    }
    EXPECT_EQ(visited.size(), 3);
    EXPECT_EQ(*queue.current(), first);
    EXPECT_TRUE(queue.previous());
    EXPECT_EQ(queue.position(), 3);
    EXPECT_TRUE(queue.ended());
    EXPECT_EQ(*queue.current(), first);
}
TEST(MusicPlaylistTest, StopsAfterOneFailedPassUntilExplicitRetry)
{
    msa::core::MusicPlaylist queue{42};
    queue.replace(tracks());
    EXPECT_TRUE(queue.failed());
    EXPECT_TRUE(queue.failed());
    EXPECT_FALSE(queue.failed());
    const auto last = *queue.current();
    EXPECT_FALSE(queue.failed());
    EXPECT_EQ(*queue.current(), last);
    queue.retry();
    EXPECT_TRUE(queue.failed());
    EXPECT_TRUE(queue.failed());
    EXPECT_FALSE(queue.failed());
    EXPECT_TRUE(queue.previous());
    EXPECT_TRUE(queue.failed());
    EXPECT_TRUE(queue.failed());
    EXPECT_FALSE(queue.failed());
}
TEST(MusicPlaylistTest, SuccessfulEndAndReplacementStartNewFailureBudget)
{
    msa::core::MusicPlaylist queue{42};
    queue.replace(tracks());
    EXPECT_TRUE(queue.failed());
    EXPECT_TRUE(queue.failed());
    EXPECT_TRUE(queue.ended());
    EXPECT_TRUE(queue.failed());
    EXPECT_TRUE(queue.failed());
    EXPECT_FALSE(queue.failed());
    queue.replace({{"/single.wav", "Один"}});
    EXPECT_FALSE(queue.failed());
    EXPECT_FALSE(queue.failed());
    EXPECT_TRUE(queue.next());
    EXPECT_FALSE(queue.failed());
    queue.replace({});
    EXPECT_EQ(queue.current(), nullptr);
}
TEST(MusicPlaylistTest, IndependentSeededQueuesAndInvalidReplacementPreserveState)
{
    msa::core::MusicPlaylist first{42};
    msa::core::MusicPlaylist second{42};
    first.replace(tracks());
    second.replace(tracks());
    EXPECT_EQ(*first.current(), *second.current());
    EXPECT_TRUE(first.next());
    const auto current = *first.current();
    EXPECT_THROW(first.replace({{"", "Нет пути"}}), std::invalid_argument);
    EXPECT_EQ(*first.current(), current);
    EXPECT_EQ(first.count(), 3);
    EXPECT_EQ(second.position(), 1);
}
} // namespace
