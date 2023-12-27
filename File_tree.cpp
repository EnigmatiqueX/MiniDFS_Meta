#include "File_tree.h"

using namespace std;

// split commands
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

// Initialize FileTree
FileTree::FileTree()
    {
        root = new TreeNode("/", false);
    }

bool FileTree::find(const string value, bool isFile, TreeNode*& node_parent)
    {
        // If the directory is root, return true
        if (value == "/") {
            return true;
        }
        // Split the path to find
        vector<string> files = split(value, '/');

        TreeNode* node = root->firstChild;

        bool isFound = true;


        for (int i = 0; i < files.size(); i++)
        {
            bool isBreakFromWhile = false;

            string file = files[i];

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
                    node = node -> firstChild;
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

void FileTree::insert(const string value, bool isFile)
    {
        TreeNode *nodeParent = root;

        // check if file path exists, at the same time update nodeparent to be the Outermost folder
        bool isFound = find(value, isFile, nodeParent);


        if (!isFound)
        {
            vector<std::string> files = split(value, '/');
            // set up the new node for the new file
            TreeNode* newNode = new TreeNode(files.back(), isFile);

            newNode->parent = nodeParent;

            TreeNode *temp = nodeParent->firstChild;

            // if the current folder has no child
            if (temp == nullptr)
            {
                nodeParent->firstChild = newNode;
            }

            // find the last sibling in the current folder
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

void FileTree::cd(string& cd_path) 
{
    TreeNode* nodeParent = root;
    bool isFound = find(cd_path, false, nodeParent);
    if (!isFound) {
        cerr << "Error: Folder not found: " << cd_path << std::endl;
        cout << "MiniDFS>";
    }
    else {
        vector<string> files = split(cd_path, '/');
        cout << "MiniDFS>";
        for (auto& file : files) {
            cout << file << ">";
        }
    }
}

void FileTree::ls(string& ls_path)
{
    TreeNode* nodeParent = root;
    bool isFound = find(ls_path, false, nodeParent);
    if (!isFound) {
        cout << "Error : Folder not found: " << ls_path << endl;
    }
    else {
        TreeNode* cur = nodeParent;
        if (cur -> firstChild == nullptr) {
            return;
        }
        else {
            cout << cur -> firstChild -> value << endl;
        }

        cur = cur -> firstChild;

        while (cur && cur -> nextSibling) {
            cur = cur -> nextSibling;
            cout << cur -> value << endl;
        }

        return;    
    }

}

void FileTree::readdir(string& readdir_path)
{
    TreeNode* nodeParent = root;
    bool isFound = find(readdir_path, false, nodeParent);
    if (!isFound) {
        cout << "Error : Folder not found: " << readdir_path << endl;
    }
    else {
        TreeNode* cur = nodeParent;
        if (cur -> firstChild == nullptr) {
            return;
        }
        else {
            cout << cur -> firstChild -> value << endl;
        }

        cur = cur -> firstChild;

        while (cur && cur -> nextSibling) {
            cur = cur -> nextSibling;
            cout << cur -> value << endl;
        }

        return;    
    }
}

void FileTree::locate(string& locate_path)
{

    TreeNode* nodeParent = root;
    bool isFound = find(locate_path, true, nodeParent);
    if (!isFound) {
        cout << "Error : File not found: " << locate_path << endl;
    }
    else {
        cout << "File " << locate_path << " exists" << endl;
    }

}

void FileTree::print(TreeNode * node)
    {
        if (node != nullptr)
        {
            cout << node->value << endl;
            print(node->nextSibling);
            print(node->firstChild);
        }
    }

void FileTree::printall()
    {
        print(root->firstChild);
    }

