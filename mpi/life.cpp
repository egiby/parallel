#include <stdio.h>

#include "MPIConnectionClient.hpp"

#include "../common/NetAlgorithm.hpp"

int main(int argc, char ** argv)
{
    IConnectionClient * connection = new MPIConnectionClient();
    
    play(connection, argc, argv);
    delete connection;
    
    return 0;
}
