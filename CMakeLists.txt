cmake_minimum_required(VERSION 3.20)
project(ClassAnalyzer)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

add_executable(ClassAnalyzer)
target_sources(ClassAnalyzer PRIVATE src/main.cpp)
target_compile_features(ClassAnalyzer PRIVATE cxx_std_20)
target_compile_definitions(ClassAnalyzer PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(ClassAnalyzer PRIVATE ${LLVM_INCLUDE_DIR})
target_link_directories(ClassAnalyzer PRIVATE ${LLVM_LIBRARY_DIR})
target_link_libraries(
  ClassAnalyzer PRIVATE clangAST clangASTMatchers clangBasic clangFrontend
                        clangSerialization clangTooling)
