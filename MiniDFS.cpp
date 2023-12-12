#include "MiniDFS.h"
using namespace std;


const int max_dataserver_num = 20;

// 2MB
constexpr double block_size = 2.0 * 1024 * 1024;

constexpr int block_size_int = 2 * 1024 * 1024;

int dataserver_num = 4;

int replicate_num = 3;

condition_variable cn_cv;
mutex cn_m;

condition_variable nd_cv;
mutex nd_m;

bool nameNotified = false;
bool finish = false;

vector<bool> dataNotified(max_dataserver_num, false);


bool isTrueCmd;

OperType type;
int fileID = 0;

int read_fileId;
string read_filename;
long long read_offset;
int read_count;

int server_executing_read;
string read_logic_file;
int read_block;
int offset_in_block;
bool is_ready_read;

string desFileName = "";

ifstream ifs;


int fetch_id;
string fetch_savepath;
string fetch_filepath;
bool is_ready_fetch;
int fetch_blocks;
const int max_blocks = 200;
int fetch_servers[max_blocks];


string mkdir_path;

FileTree* tree = new FileTree();

// for therad communication: <serverid, range{from, count}>
// note that one serverid can hold server ranges
multimap<int, fileRange> server_fileRangesMap;


/**
 * metadata:
 *
比如, /home/a.txt 可能被分为3个block,需要知道每个block在哪?该block对应实际存储名
a-part0在0, 1; a-part1在1,2; a-part2在 2,3

<"/home/a.txt", "a.txt-part0, a.txt-part1">
<"a.txt-part0", "1, 2, 3" >

 */

// for metadata: <fildId, [logicFilePath, length]>
map<int, pair<string, long long>> fileid_path_lenMap;
map<string, long long> path_lenMap;


// for metadata: <logicFilePath, blockfiles>
map<string, vector<string>> logicFile_BlockFileMap;

// for metadata: <blockfile, serverid>
map<string, vector<int>> block_serversMap;

bool getCmd(const string& cmd)
{

    vector<string> possibleCmds = {"put", "read", "mkdir", "put2", "read2", "quit", "fetch", "fetch2"};

    vector<string> x = split(cmd, ' ');

    // put
    if (x[0].compare(possibleCmds[0]) == 0)
    {
        type = OperType::put;
        if (x.size() != 2)
        {
            cerr << "Error : Usage: put source_file_path" << endl;
            return false;
        }
        string sourcePath = x[1];
        ifs.open(sourcePath);
        if (! ifs.is_open())
        {
            cerr << "Error: the input file does not exist" << endl;
            return false;
        }

        boost::filesystem::path p(sourcePath);

        desFileName += p.filename().string();

    }

    // read
    else if (x[0].compare(possibleCmds[1]) == 0)
    {
        type = OperType::read;
        if (x.size() != 4)
        {
            cerr << "Usage: read file_id offset count" << endl;
            return false;
        }

        try {
            read_fileId = stoi(x[1]);
            read_offset = stoll(x[2]);
            read_count = stoi(x[3]);
        }
        catch (exception& e)
        {
            std::cout << "Error: id, offset or count isn't a number" << std::endl;
            return false;
        }

    }

    // mkdir
    else if(x[0].compare(possibleCmds[2]) == 0){
        type = OperType::mkdir;
        if (x.size() != 2)
        {
            cerr << "Error: Usage: mkdir folder" << endl;
            return false;
        }
        mkdir_path = x[1];

    }
    // put2
    else if(x[0].compare(possibleCmds[3]) == 0)
    {
        type = OperType::put2;
        if (x.size() != 3)
        {
            cerr << "Error: Usage: put2 source_file_path des_file_folder" << endl;
            return false;
        }
        string sourcePath = x[1];
        ifs.open(sourcePath);
        if (! ifs.is_open())
        {
            cerr << "Error: the input file does not exist" << endl;
            return false;
        }

        desFileName = x[2];

    }

    // read2
    else if (x[0].compare(possibleCmds[4]) == 0)
    {
        type = OperType::read2;
        if (x.size() != 4)
        {
            cerr << "Error: Usage: read2 file_path offset count" << endl;
            return false;
        }

        read_filename = x[1];

        try {
            read_offset = stoll(x[2]);
            read_count = stoi(x[3]);
        }
        catch (exception& e)
        {
            std::cout << "Error: offset or count isn't a number" << std::endl;
            return false;
        }

    }
    // Exit
    else if (x[0].compare(possibleCmds[5]) == 0)
    {
        cout << "Exit MiniDFS" << endl;
        exit(0);
    }

    // fetch
    else if (x[0].compare(possibleCmds[6]) == 0)
    {
        type = OperType::fetch;
        // fetch files
        if (x.size() != 3)
        {
            cerr << "Usage: fetch file_id save_path" << endl;
            return false;
        }

        try {
            fetch_id = stoi(x[1]);
            fetch_savepath = x[2];
        }
        catch (exception& e)
        {
            std::cout << "Error: id isn't a number" << std::endl;
            return false;
        }

    }
    // fetch2
    else if (x[0].compare(possibleCmds[7]) == 0)
    {
        type = OperType::fetch2;
        // fetch files
        if (x.size() != 3)
        {
            cerr << "Usage: fetch2 file_path save_path" << endl;
            return false;
        }

        fetch_filepath = x[1];
        fetch_savepath = x[2];
    }

    return true;
}