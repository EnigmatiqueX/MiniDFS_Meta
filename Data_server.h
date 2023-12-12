#ifndef _DATA_SERVER_H
#define _DATA_SERVER_H

#include "MiniDFS.h"

class DataServer
{
public:

    explicit DataServer(int _serverId);
    void operator()()const;

private:

    int serverId;

    void saveFile() const;
    void readFileAndOutput() const;
    void mkdir() const;
};



#endif