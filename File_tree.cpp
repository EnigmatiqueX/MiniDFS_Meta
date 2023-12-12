#include "File_tree.h"

using namespace std;

vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

FileTree::FileTree()
    {
        root = new TreeNode("/", false);
    }

bool FileTree::find(const std::string value, bool isFile, TreeNode*& node_parent)
    {
        std::vector<std::string> files = split(value, '/');

        TreeNode* node = root->firstChild;

        bool isFound = true;


        for (int i = 0; i < files.size(); i++)
        {
            bool isBreakFromWhile = false;

            std::string file = files[i];

            bool _isFile = isFile;
            if (i < files.size() - 1)
            {
                _isFile = false;
            }

            while (node != nullptr)
            {
                if (node->isFile == _isFile && file.compare(node->value) == 0)
                {
                    node_parent = node;
                    node = node->firstChild;
                    isBreakFromWhile = true;
                    break;
                }
                node = node->nextSibling;
            }
            if (!isBreakFromWhile)
            {
                if (node == nullptr)
                {
                    isFound = false;
                    break;
                }
            }

        }
        return isFound;
    }

void FileTree::insert(const std::string value, bool isFile)
    {
        TreeNode *nodeParent = root;

        //bool isFound = findNode(value, isFile, nodeParent);
        bool isFound = find(value, isFile, nodeParent);


        if (!isFound)
        {
            std::vector<std::string> files = split(value, '/');
            TreeNode* newNode = new TreeNode(files.back(), isFile);

            newNode->parent = nodeParent;

            TreeNode *temp = nodeParent->firstChild;

            if (temp == nullptr)
            {
                nodeParent->firstChild = newNode;
            }

            else
            {
                while (temp->nextSibling != nullptr)
                {
                    temp = temp->nextSibling;
                }

                temp->nextSibling = newNode;


            }
        }
    }

void FileTree::print(TreeNode * node)
    {
        if (node != nullptr)
        {
            std::cout << node->value << std::endl;
            print(node->nextSibling);
            print(node->firstChild);
        }
    }

void FileTree::printall()
    {
        print(root->firstChild);
    }

