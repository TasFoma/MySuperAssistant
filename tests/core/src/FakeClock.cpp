#include "FakeClock.h"

namespace msa::core {

FakeClock::FakeClock(std::chrono::local_time<std::chrono::seconds> moment)
    : m_moment{moment}
{
}

std::chrono::local_time<std::chrono::seconds> FakeClock::now() const
{
    return m_moment;
}

void FakeClock::set(std::chrono::local_time<std::chrono::seconds> moment)
{
    m_moment = moment;
}

void FakeClock::advance(std::chrono::seconds elapsed)
{
    m_moment += elapsed;
}

}  // namespace msa::core

