project(python-agents)

find_package(Python 3.8 REQUIRED COMPONENTS Interpreter)

add_test (NAME python-tests
  COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

set_tests_properties(python-tests PROPERTIES
    ENVIRONMENT "ARCHIVE_LIB_DIR=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
)