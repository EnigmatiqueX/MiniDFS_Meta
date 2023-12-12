#ifndef _NAME_SERVER_H
#define _NAME_SERVER_H


#include "MiniDFS.h"

class NameServer
{
public:
    void operator()()const;

private:
    void StoreBlockInfo() const;
    bool assignReadWork() const;
    bool assignFetchWork() const;
    void loadMeta() const;
    void writeMeta(MetaType type) const;
};

#endif