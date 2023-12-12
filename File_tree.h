
#ifndef _FILE_TREE_H
#define _FILE_TREE_H
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <iostream>

using namespace std;

vector<string> &split(const string &s, char delim, vector<string> &elems);

vector<string> split(const string &s, char delim);

struct fileRange
{
    long long from;
    int count;
    int blockId;
    fileRange(long long _from, int _count, int _blockId)
    {
        from = _from;
        count = _count;
        blockId = _blockId;
    }
};


struct TreeNode
{
    std::string value;
    bool isFile;
    TreeNode* parent;
    TreeNode* firstChild;
    TreeNode* nextSibling;
    TreeNode(std::string _value, bool _isFile): value(_value), isFile(_isFile), parent(nullptr), firstChild(nullptr), nextSibling(
            nullptr) {}
};

class FileTree
{
public:
    TreeNode *root = nullptr;
    bool find(const std::string value, bool isFile, TreeNode*& node_parent);

public:
    FileTree();
    void insert(const std::string value, bool isFile);
    void print(TreeNode * node);
    void printall();

};

#endif 
