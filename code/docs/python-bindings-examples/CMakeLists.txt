cmake_minimum_required(VERSION 3.16)

project(Python-Bindings LANGUAGES C)

add_library(bindings SHARED)   # The library has to be SHARED
target_sources(bindings PRIVATE src.c)
