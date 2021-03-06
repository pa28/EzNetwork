cmake_minimum_required(VERSION 3.9)
project(EzNetwork)

set(CMAKE_CXX_STANDARD 17)

find_package (Threads)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( doc_doxygen
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM )
else (DOXYGEN_FOUND)
    message("Doxygen needs to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)

add_executable(ServerTest serverTest.cpp socket.h server.h name_that_type.h socket_buffer.h)

add_executable(ManipTest iomanip.h manipTest.cpp name_that_type.h)

add_executable(AsyncServer asyncServerTest.cpp socket.h server.h name_that_type.h socket_buffer.h)

target_link_libraries (AsyncServer ${CMAKE_THREAD_LIBS_INIT})

add_executable(AsyncNet basic_socket.h asyncNet.cpp socket_buffer.h)

target_link_libraries (AsyncNet ${CMAKE_THREAD_LIBS_INIT})
