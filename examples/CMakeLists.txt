add_executable( dense_inference util.cpp dense_inference.cpp )
add_executable( dense_binary util.cpp dense_binary.cpp)
target_link_libraries( dense_inference densecrf )
target_link_libraries( dense_binary densecrf )
