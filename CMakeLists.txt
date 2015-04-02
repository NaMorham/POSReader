cmake_minimum_required(VERSION 2.8)

project( PosReader )

set( CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++11 )

option( CMAKE_INSTALL_PREFIX "Override CMAKE_INSTALL_PREFIX" ${CMAKE_INSTALL_PREFIX} )

set( SRCS main.cpp )

add_executable( PosReader ${SRCS} )

install( TARGETS PosReader
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

