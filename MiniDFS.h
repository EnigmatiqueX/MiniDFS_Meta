#ifndef _MINIDFS_H
#define _MINIDFS_H

#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <exception>
#include <cmath>
#include <vector>
#include <map>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>

#include <boost/filesystem.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include "File_tree.h"

using namespace std;

extern const int max_dataserver_num;

// 2MB
extern const double block_size;

extern const int block_size_int;

extern int dataserver_num;

extern int replicate_num;


// Communication between client and nameserver
extern condition_variable cn_cv;
extern mutex cn_m;

// Communication between nameserver and dataserver
extern condition_variable nd_cv;
extern mutex nd_m;


// Notify the Namenode to work
extern bool nameNotified;
extern bool finish;

extern vector<bool> dataNotified;

extern bool isTrueCmd;

enum class OperType {put,read,mkdir,put2,read2,fetch,fetch2};

enum class MetaType {id_file, file_block, block_server, current_id, file_len};

extern OperType type;
extern int fileID;

extern int read_fileId;
extern string read_filename;
extern long long read_offset;
extern int read_count;

extern int server_executing_read;
extern string read_logic_file;
extern int read_block;
extern int offset_in_block;
extern bool is_ready_read;

extern string desFileName;

extern ifstream ifs;


extern int fetch_id;
extern string fetch_savepath;
extern string fetch_filepath;
extern bool is_ready_fetch;
extern int fetch_blocks;
extern const int max_blocks;
extern int fetch_servers[200];


extern string mkdir_path;

extern FileTree* tree;

// for therad communication: <serverid, range{from, count}>
// note that one serverid can hold server ranges
extern multimap<int, fileRange> server_fileRangesMap;


/**
 * metadata:
 *
比如, /home/a.txt 可能被分为3个block,需要知道每个block在哪?该block对应实际存储名
a-part0在0, 1; a-part1在1,2; a-part2在 2,3

<"/home/a.txt", "a.txt-part0, a.txt-part1">
<"a.txt-part0", "1, 2, 3" >

 */

// for metadata: <fildId, [logicFilePath, length]>
extern map<int, pair<string, long long>> fileid_path_lenMap;
extern map<string, long long> path_lenMap;


// for metadata: <logicFilePath, blockfiles>
extern map<string, vector<string>> logicFile_BlockFileMap;

// for metadata: <blockfile, serverid>
extern map<string, vector<int>> block_serversMap;

extern bool getCmd(const string& cmd);

#endif



