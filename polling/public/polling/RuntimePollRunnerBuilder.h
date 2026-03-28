#pragma once

#include "polling/PollableConcept.h"
#include "polling/RuntimePollRunner.h"

namespace polling
{

    class RuntimePollRunnerBuilder
    {
        public:
            RuntimePollRunnerBuilder() = default;

            template <CPollableBuilder Builder_T>
            RuntimePollRunnerBuilder& add_pollable(Builder_T&& builder)
            {
                pollable_builders.emplace_back(
                    [b = std::forward<Builder_T>(builder)]() mutable {
                        return b();
                    }
                );
                return *this;
            }

            std::unique_ptr<IPollRunner> build_runner() {
                return std::make_unique<RuntimePollRunner>(pollable_builders);
            }

        private:
            // vector of lambdas that take a memory location and construct a pollable in place
            std::vector<PollableFactory> pollable_builders;
    };

}