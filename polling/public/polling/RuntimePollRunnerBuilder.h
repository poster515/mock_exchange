
#include "PollableConcept.h"

namespace polling
{
    template <typename...PollableBuilders>
        requires (CPollableBuilder<PollableBuilders> && ...)
    class RuntimePollRunnerBuilder
    {
        public:
            RuntimePollRunnerBuilder() = default;

            template <CPollableType Pollable_T> 
            RuntimePollRunnerBuilder& add_pollable(Pollable_T pollable)
            {
                pollables.emplace_back(std::move(pollable));
                return *this;
            } 

        private:
            // vector of lambdas that take a memory location and construct a pollable in place
            std::vector<PollableBuilders...> pollable_builders;
    };

}