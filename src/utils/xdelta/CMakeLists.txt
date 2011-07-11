cmake_minimum_required (VERSION 2.6)
project (xdelta)
find_package(Qt4 REQUIRED)

set(XDELTA_SRC main.cpp xdelta.cpp)
set(XDELTA_HEADERS xdelta.h)
set(XDELTA_FORMS xdelta.ui)

qt4_wrap_cpp(XDELTA_HEADERS_MOC ${XDELTA_HEADERS})
qt4_wrap_ui(XDELTA_FORMS_HEADERS ${XDELTA_FORMS})


include(${QT_USE_FILE})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(${QT_DEFINITIONS})
add_executable(xdelta ${XDELTA_SRC} ${XDELTA_HEADERS_MOC} ${XDELTA_FORMS_HEADERS})
target_link_libraries (xdelta ${QT_LIBRARIES})

install (TARGETS xdelta DESTINATION bin)
