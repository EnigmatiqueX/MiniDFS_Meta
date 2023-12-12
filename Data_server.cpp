#include "Data_server.h"

using namespace std;

DataServer::DataServer(int _serverId)
    {
        serverId = _serverId;
    }

void DataServer::operator() () const
    {
        for (;;)
        {
            unique_lock<mutex> lk(nd_m);
            nd_cv.wait(lk, [this]{return dataNotified[serverId];});

            if (isTrueCmd && (type == OperType::put || type == OperType::put2))
            {
                saveFile();
            }
            else if (isTrueCmd && (type == OperType::read || type == OperType::read2)){

                if (is_ready_read && server_executing_read == serverId)
                {
                    //cout << "is ready read and my id is " << serverId << endl;
                    readFileAndOutput();
                }
            }
            else if (isTrueCmd && type == OperType::mkdir)
            {
                mkdir();
            }
            //cout << serverId << " data notified \n";
            dataNotified[serverId] = false;
            lk.unlock();
            nd_cv.notify_all();
        }
    }

void DataServer::saveFile() const
    {
        // dfs/datanode[1,2,3,4]/
        // check the server_fileRangesMap
        // <serverid, range{from, count, blockid}>
        auto fileRanges = server_fileRangesMap.equal_range(serverId);
        string nodePath = "DFSfiles/DataNode " + to_string(serverId);
        for (auto f_it = fileRanges.first; f_it!= fileRanges.second; ++f_it)
        {
            int _blockId = (f_it->second).blockId;
            long long _from = (f_it->second).from;
            int _count = (f_it->second).count;
            char *buffer = new char[block_size_int];
            ifs.seekg(_from, ifs.beg);

            ifs.read(buffer, _count);

            ifs.seekg(0, ifs.beg);

            ofstream myblockFile;

            myblockFile.open(nodePath + "/" + desFileName + "-part" + to_string(_blockId));

            myblockFile.write(buffer, _count);

            myblockFile.close();

            delete[] buffer;
        }


    }

void DataServer::readFileAndOutput() const
    {
        string blockpath = "DFSfiles/DataNode " + to_string(serverId) + "/"
        + read_logic_file + "-part" + to_string(read_block);

        ifstream blcokFile(blockpath);

        if (blcokFile.is_open())
        {
            char *buffer = new char[block_size_int];
            blcokFile.seekg(offset_in_block, blcokFile.beg);
            blcokFile.read(buffer, read_count);

            cout.write(buffer, read_count);
            cout << endl;

            delete[] buffer;
            blcokFile.close();
        }
        else
        {
            cerr << "Failed to open block" << endl;
        }
    }

// Create folder
void DataServer::mkdir() const
    {
        string folder = "DFSfiles/DataNode " + to_string(serverId) + "/" + mkdir_path;
        boost::filesystem::create_directories(folder);
    }