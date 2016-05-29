cmake_minimum_required (VERSION 2.6)
project (CL342)

MESSAGE( STATUS "Using CryptLib: ${CL342_LIB}" )

find_library(CL342_LIB libcl.a PATHS .)



