# build.bash
#
# Helper commands for build/test for WWIV
#

# Runs a single test collecting the output into
# ${test_name}.xml (i.e. core-test.xml)
#
# Args:
# 1: test dir i.e. ${WORKSPACE}/core_tests
# 2: test name, no suffic (i.e. core-test)
run_test() {
    set +e
    local test_dir=$1
    local test_name=$2
    if [[ -z "${test_dir}" ]]; then
	echo "test_dir must be specified to run_test"
	exit 1;
    fi
    if [[ -z "${test_name}" ]]; then
	echo "test_name must be specified to run_test"
	exit 1;
    fi

    cd "${test_dir}"
    if [[ -r result.xml ]]; then
	rm result.xml
    fi

    ./${test_name} --gtest_output=xml:result-${test_name}.xml
    cd ${WORKSPACE}
    set -e
}

# Args:
# $1 make args

build_binaries() {
    local make_args=$1
    
    echo "Building binaries"
    cd ${WORKSPACE}
    sed -i -e "s@.development@.${BUILD_NUMBER}@" core/version.cpp
    
    echo "Compiling dependencies that are not CMake friendly"
    pushd deps/cl342
    make ${make_args}
    popd > /dev/null
    
    echo "Compiling Everything"
    if [[ ! -d "${CMAKE_BUILD}" ]]; then
	mkdir -p ${CMAKE_BUILD}
    fi
    
    pushd ${CMAKE_BUILD}
    ${CMAKE_BIN} -DCMAKE_BUILD_TYPE:STRING=Debug ..
    ${CMAKE_BIN} --build . -- ${make_args}
    popd > /dev/null

}
