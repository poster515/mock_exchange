project(python-agents)

find_package(Python 3.8 REQUIRED COMPONENTS Interpreter)

add_test (NAME python-tests
  COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)