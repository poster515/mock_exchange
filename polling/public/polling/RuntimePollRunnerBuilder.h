#pragma once

#include "polling/PollableConcept.h"
#include "polling/RuntimePollRunner.h"

namespace polling
{
    template <typename...PollableBuilders>
        requires (CPollableBuilder<PollableBuilders> && ...)
    class RuntimePollRunnerBuilder
    {
        public:
            RuntimePollRunnerBuilder() = default;

            template <CPollableBuilder Builder_T> 
            RuntimePollRunnerBuilder& add_pollable(Builder_T pollable)
            {
                pollables.emplace_back(std::move(pollable));
                return *this;
            }

            std::unique_ptr<IPollRunner> build_runner() {
                return std::make_unique<RuntimePollRunner>(pollable_builders);
            }

        private:
            // vector of lambdas that take a memory location and construct a pollable in place
            std::vector<PollableBuilders...> pollable_builders;
    };

}