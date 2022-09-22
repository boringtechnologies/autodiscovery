cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

project(grpc-roc LANGUAGES CXX)

include(cmake/common.cmake)


#if(NOT APPLE)
#    set(BUILD_SHARED_LIBS OFF)
#    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
#endif()

# set(ZLIB_USE_STATIC_LIBS "ON")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_package(ZLIB REQUIRED)

find_package(LibJuice REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED
        gobject-2.0
        glib-2.0
        gstreamer-1.0
        gstreamer-app-1.0
)

# Proto files
get_filename_component(grpc_roc_proto "common/proto/grpc-roc.proto" ABSOLUTE)
get_filename_component(grpc_roc_proto_path "${grpc_roc_proto}" PATH)

set(grpc_roc_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/grpc-roc.pb.cc")
set(grpc_roc_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/grpc-roc.pb.h")
set(grpc_roc_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/grpc-roc.grpc.pb.cc")
set(grpc_roc_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/grpc-roc.grpc.pb.h")
add_custom_command(
      OUTPUT "${grpc_roc_proto_srcs}" "${grpc_roc_proto_hdrs}" "${grpc_roc_grpc_srcs}" "${grpc_roc_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${grpc_roc_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${grpc_roc_proto}"
      DEPENDS "${grpc_roc_proto}")

include_directories(${CMAKE_SOURCE_DIR}/media/include)

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(grpc_roc_proto
    ${grpc_roc_grpc_srcs}
    ${grpc_roc_grpc_hdrs}
    ${grpc_roc_proto_srcs}
    ${grpc_roc_proto_hdrs})

target_link_libraries(grpc_roc_proto
    ${ZLIB_LIBRARIES}
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

add_executable(grpc-roc-server server/main.cpp)
target_link_libraries(grpc-roc-server
                      grpc_roc_proto
                      ${_REFLECTION}
                      ${_GRPC_GRPCPP}
                      ${_PROTOBUF_LIBPROTOBUF})

#if(APPLE)
    set(JUICE_LIBRARY LibJuice::LibJuice)
#else()
#    set(JUICE_LIBRARY juice-static)
#endif()

add_executable(grpc-roc-client
               client/main.cpp
               client/grpcclient.h
               client/grpcclient.cpp
               client/juiceclient.h
               client/juiceclient.cpp
               media/include/sync.hpp
               media/src/internal.hpp
               media/src/receiver.cpp
               media/src/sender.cpp)
target_include_directories(grpc-roc-client PRIVATE
                           ${GST_INCLUDE_DIRS})
target_link_directories(grpc-roc-client PRIVATE
                        ${GST_LIBRARY_DIRS})

target_link_libraries(grpc-roc-client PRIVATE
                      grpc_roc_proto
                      ${GST_LIBRARIES}
                      ${JUICE_LIBRARY}
                      ${_REFLECTION}
                      ${_GRPC_GRPCPP}
                      ${_PROTOBUF_LIBPROTOBUF})

set(OTHER_FILES common/proto/grpc-roc.proto common/initial.txt README.md)

set_source_files_properties(${OTHER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
add_custom_target(project-related-files SOURCES ${OTHER_FILES})
