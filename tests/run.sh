#!/bin/sh
# Builds and runs the unit tests of the messages format (no BakkesMod SDK needed)
# Requires nlohmann/json (e.g. apt install nlohmann-json3-dev), extra include paths can be given in CXXFLAGS
set -e
cd "$(dirname "$0")/.."
mkdir -p build/tests
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -DFMT_HEADER_ONLY $CXXFLAGS -IRLTMBridge -IRLTMBridge/fmt/include \
	tests/ProtocolTests.cpp RLTMBridge/Protocol/Protocol.cpp -o build/tests/ProtocolTests
./build/tests/ProtocolTests
