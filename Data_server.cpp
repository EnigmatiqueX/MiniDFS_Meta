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
            // set up a lock for each dataserver thread;
            unique_lock<mutex> lk(nd_m);
            nd_cv.wait(lk, [this]{return dataNotified[serverId];});

            // Save file on each dataserver;
            if (isTrueCmd && (type == OperType::put || type == OperType::put2))
            {
                saveFile();
            }
            // Read file on the given dataserver by nameserver
            else if (isTrueCmd && (type == OperType::read || type == OperType::read2)){

                if (is_ready_read && server_reading == serverId)
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
            // continue the nameserver thread
            dataNotified[serverId] = false;
            lk.unlock();
            nd_cv.notify_all();
        }
    }

void DataServer::saveFile() const
    {
        // DFSfiles/DataNode[0,1,2,3]/
        // check the server_fileRangesMap
        // <serverid, range{start, count, blockid}>
        // key = serverId
        auto fileRanges = server_fileRangesMap.equal_range(serverId);
        string nodePath = "DFSfiles/DataNode_" + to_string(serverId);

        // Traverse all <serverid, range{start, count, blockid}> with given serverId
        for (auto f_it = fileRanges.first; f_it!= fileRanges.second; ++f_it)
        {
            int _blockId = (f_it->second).blockId;
            long long _from = (f_it->second).from;
            int _count = (f_it->second).count;
            char *buffer = new char[block_size_int];
            // read data to filestream;
            ifs.seekg(_from, ifs.beg);

            ifs.read(buffer, _count);

            ifs.seekg(0, ifs.beg);

            // Write data to desfile
            ofstream myblockFile;
            myblockFile.open(nodePath + "/" + desFileName + "-part" + to_string(_blockId));

            myblockFile.write(buffer, _count);

            myblockFile.close();

            delete[] buffer;
        }


    }

void DataServer::readFileAndOutput() const
    {
        string blockpath = "DFSfiles/DataNode_" + to_string(serverId) + "/"
        + read_logic_file + "-part" + to_string(read_block);

        ifstream blockFile(blockpath);
        // Fail to open the file
        if (!blockFile.is_open())
        {
            cerr << "Failed to open block: " << blockpath << std::endl;
            return;
        }
        char *buffer = new char[block_size_int];
        blockFile.seekg(offset_in_block, blockFile.beg);
        blockFile.read(buffer, read_count);

        cout.write(buffer, read_count);
        cout << endl;

        delete[] buffer;
        blockFile.close();
    }

// Create folder
void DataServer::mkdir() const
    {
        string folder = "DFSfiles/DataNode_" + to_string(serverId) + "/" + mkdir_path;
        boost::filesystem::create_directories(folder);
    }
