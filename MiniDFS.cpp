#include "MiniDFS.h"
using namespace std;


const int max_dataserver_num = 20;

// 2MB
constexpr double block_size = 2.0 * 1024 * 1024;

constexpr int block_size_int = 2 * 1024 * 1024;

// Four dataservers and three replications for each chunk
int dataserver_num = 4;

int replicate_num = 3;

// Lock and conditional variable between client and nameserver
condition_variable cn_cv;
mutex cn_m;

// Lock and conditional variable between nameserver and dataserver
condition_variable nd_cv;
mutex nd_m;

// Judge if nameserver is notified and finished its work
bool nameNotified = false;
bool finish = false;

// Judge if dataserver is notified
vector<bool> dataNotified(max_dataserver_num, false);

// Cmd is true or not
bool isTrueCmd;

//
OperType type;
int fileID = 0;

// Read info
int read_fileId;
string read_filename;
long long read_offset;
int read_count;

int server_reading;
string read_logic_file;
int read_block;
int offset_in_block;
bool is_ready_read;

string desFileName = "";

ifstream ifs;

// fetch info
int fetch_id;
string fetch_savepath;
string fetch_filepath;
bool is_ready_fetch;
int fetch_blocks;
const int max_blocks = 200;
int fetch_servers[max_blocks];

// mkdir info
string mkdir_path;

// cd info
string cd_path;

// ls path
string ls_path;

// locate path
string locate_path;

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

    vector<string> possibleCmds = {"put", "read", "mkdir", "put2", "read2", "quit", "fetch", "fetch2", "cd", "ls", "locate"};

    vector<string> x = split(cmd, ' ');

    // put : put source_file_path
    if (x[0].compare(possibleCmds[0]) == 0)
    {
        type = OperType::put;
        if (x.size() != 2)
        {
            cerr << "Error : Usage: put source_file_path" << endl;
            return false;
        }
        string sourcePath = x[1];
        // ifs input file
        ifs.open(sourcePath);
        if (!ifs.is_open())
        {
            cerr << "Error: the input file does not exist" << endl;
            return false;
        }

        boost::filesystem::path p(sourcePath);
        // Get file name
        desFileName += p.filename().string();
        tree -> insert(desFileName, true);
        cout << desFileName << endl;

    }

    // read : read file_id offset count
    else if (x[0].compare(possibleCmds[1]) == 0)
    {
        type = OperType::read;
        if (x.size() != 4)
        {
            cerr << "Usage: read file_id offset count" << endl;
            return false;
        }

        // Store file info for reading
        try {
            read_fileId = stoi(x[1]);
            read_offset = stoll(x[2]);
            read_count = stoi(x[3]);
        }
        catch (exception& e)
        {
            cerr << "Error: id, offset or count isn't a number" << endl;
            return false;
        }

    }

    // mkdir : mkdir mkdir_path
    else if(x[0].compare(possibleCmds[2]) == 0){
        type = OperType::mkdir;
        if (x.size() != 2)
        {
            cerr << "Error: Usage: mkdir folder" << endl;
            return false;
        }
        mkdir_path = x[1];

    }
    // put2 : put2 source_file_path des_file_path
    else if(x[0].compare(possibleCmds[3]) == 0)
    {
        type = OperType::put2;
        if (x.size() != 3)
        {
            cerr << "Error: Usage: put2 source_file_path des_file_folder" << endl;
            return false;
        }
        string sourcePath = x[1];
        // ifs input file
        ifs.open(sourcePath);
        if (! ifs.is_open())
        {
            cerr << "Error: the input file does not exist" << endl;
            return false;
        }

        desFileName = x[2];
        tree -> insert(desFileName, true);
    }

    // read2 : read2 file_path offset count
    else if (x[0].compare(possibleCmds[4]) == 0)
    {
        type = OperType::read2;
        if (x.size() != 4)
        {
            cerr << "Error: Usage: read2 file_path offset count" << endl;
            return false;
        }

        // Store file info for reading
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

    // fetch : fetch file_id save_path
    else if (x[0].compare(possibleCmds[6]) == 0)
    {
        type = OperType::fetch;
        // fetch files
        if (x.size() != 3)
        {
            cerr << "Error: Usage: fetch file_id save_path" << endl;
            return false;
        }

        // store file info for fetch
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

    // fetch2 : fetch2 file_path save_path
    else if (x[0].compare(possibleCmds[7]) == 0)
    {
        type = OperType::fetch2;
        // fetch files
        if (x.size() != 3)
        {
            cerr << "Error: Usage: fetch2 file_path save_path" << endl;
            return false;
        }

        // store file info for fetch
        
        fetch_filepath = x[1];
        fetch_savepath = x[2];
    }

    // cd : cd sub_folder
    else if (x[0].compare(possibleCmds[8]) == 0)
    {
        type = OperType::cd;
        if (x.size() != 2) {
            cerr << "Error: Usage: cd sub_folder" << endl;
            return false;
        }
        cd_path = x[1];

    }

    // ls : ls
    else if (x[0].compare(possibleCmds[9]) == 0)
    {
        type = OperType::ls;
        if (x.size() != 2) {
            cerr << "Error: Usage: ls ls_path" << endl;
            return false;
        }

        ls_path = x[1];
    }

    // locate : locate locate_path
    else if (x[0].compare(possibleCmds[10]) == 0)
    {
        type = OperType::locate;
        if (x.size() != 2) {
            cerr << "Error: Usage: locate locate_path" << endl;
            return false;
        }

        locate_path = x[1];
    }

    return true;
}
