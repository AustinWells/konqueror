include(ECMAddTests)

find_package(Qt5Test ${QT_MIN_VERSION} CONFIG REQUIRED)

macro(webenginepart_unit_tests)
  foreach(_testname ${ARGN})
    ecm_add_test(${_testname}.cpp TEST_NAME ${_testname}
      LINK_LIBRARIES kwebenginepartlib Qt5::Test)
  endforeach()
endmacro(webenginepart_unit_tests)

webenginepart_unit_tests(
  webengine_partapi_test
)
