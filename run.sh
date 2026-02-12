echo "starting compiling"
g++ --debug -o bin/nexus.x src/main.cpp
echo "Executing Code"
./bin/nexus.x ../Examples/test.nx
