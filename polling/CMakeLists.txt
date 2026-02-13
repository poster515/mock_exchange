project(polling)

set(public_include_folder "public/polling")
set(includes
    ${public_include_folder}/IPollRunnner.h
    ${public_include_folder}/PollableConcept.h
    ${public_include_folder}/RuntimePollRunnerBuilder.h
)



add_library(polling INTERFACE)



