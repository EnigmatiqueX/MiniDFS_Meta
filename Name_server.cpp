#include "Name_server.h"

#include <iostream>
#include <fstream>
#include <ctime>
#include <string>

using namespace std;

// operater()() for thread execution
void NameServer::operator() () const
    {
        // load metadata before
        loadMeta();

        for (;;)
        {
            // Wait for client
            unique_lock<mutex> cn_lk(cn_m);
            cn_cv.wait(cn_lk, []{return nameNotified;});

            //cout << "name notified \n";

            // Store block info in NameNode
            if (isTrueCmd && (type == OperType::put || type == OperType::put2))
            {
                StoreBlockInfo();

            }
            // Assign read work
            else if (isTrueCmd && type == OperType::read)
            {
                
                is_ready_read = assignReadWork();
            }
            // Assign fetch work
            else if (isTrueCmd && (type == OperType::fetch || type == OperType::fetch2))
            {
                is_ready_fetch = assignFetchWork();
            }
            // Insert the directory
            else if (isTrueCmd && type == OperType::mkdir)
            {
                tree -> insert(mkdir_path, false);
            }
            // Go into the subfolder
            else if (isTrueCmd && type == OperType::cd)
            {
                tree -> cd(cd_path);
            }
            // List the files and folders inside the current folder
            else if (isTrueCmd && type == OperType::ls)
            {
                tree -> ls(ls_path);
            }
            // List the files and folders inside the current folder
            else if (isTrueCmd && type == OperType::locate)
            {
                tree -> locate(locate_path);
            }
            // List the files and folders inside the current folder
            else if (isTrueCmd && type == OperType::readdir)
            {
                // tree -> readdir(readdir_path);
                ;
            }
            // Print the metadata
            else if (isTrueCmd && type == OperType::stat)
            {
                ;
            }
            // Assign check work
            else if (isTrueCmd && (type == OperType::check))
            {
                ;
            }
            


            // Restore Namenotified 
            nameNotified = false;

            // Set all datanotifiled to be true
            fill_n(dataNotified.begin(), dataserver_num, true);
            // Wake up dataserver
            nd_cv.notify_all();

            unique_lock<mutex> nd_lk(nd_m);
            // Wait until all datanotifiled to be false
            nd_cv.wait(nd_lk, []{
                return all_of(dataNotified.begin(),
                              dataNotified.begin() + dataserver_num,
                              [](bool b){return !b;});
            });
            nd_lk.unlock();

            // Continue the main thread
            finish = true;
            cn_lk.unlock();
            cn_cv.notify_all();
        }
    }

void NameServer::StoreBlockInfo() const
    {
        // Calcullate block numbers
        ifs.seekg (0, ifs.end);
        long long length = ifs.tellg();
        ifs.seekg (0, ifs.beg);
        int blockNum = (int)ceil(length / block_size);

        // Metadata: key = fileId, value = logicFilePath
        fileid_path_lenMap.emplace(make_pair(fileID, make_pair(desFileName, length)));
        path_lenMap.emplace(make_pair(desFileName, length));

        // fileid_path_lenMapdate fileId
        fileID++;

        std::time_t currentTime = std::time(nullptr);
        std::string timeStamp = std::ctime(&currentTime);

        // Store block path 
        vector<string> blockPathList;
        for (int i = 0; i < blockNum; i++)
        {
            blockPathList.push_back(desFileName + "-part" + to_string(i));
        }
        // <logicFilePath, blockfiles>
        logicFile_BlockFileMap.emplace(make_pair(desFileName, blockPathList));

        for (int i = 0; i < blockNum; i++)
        {

            vector<int> _serverids;
            _serverids.push_back(i % dataserver_num);

            fileRange range(i * block_size_int, 0, i);

            // Calculate the size of last block
            if (i == blockNum - 1)
            {
                range.count = length - i * block_size_int;
            }
            else
            {
                range.count = block_size_int;
            }
            // range:{start, count, blockid}
            // Store block inside dataserver_name, blockrange 
            server_fileRangesMap.emplace(make_pair(i % dataserver_num, range));
            // Create another two replications
            for (int j = 1; j < replicate_num; j++)
            {
                // Replications are places inside two dataservers after the current dataserver (within a circle)
                // eg: {{0, range}, {1, range}, {2, range}}
                server_fileRangesMap.emplace(make_pair((i + j) % dataserver_num, range));
                // Record the three dataserver names
                _serverids.push_back((i + j) % dataserver_num);
            }

            // eg: {a.txt-part0, {0,1,2}}
            block_serversMap.emplace(make_pair(desFileName + "-part" + to_string(i),
               _serverids));
        }

        // write metadata info to file
        // {fileid, {path, len}}
        writeMeta(MetaType::id_file);
        // {path, len}
        writeMeta(MetaType::file_len);
        // {desFileName, blockPathList}
        writeMeta(MetaType::file_block);
        // {desFileName + "-part" + to_string(i), _serverids}
        writeMeta(MetaType::block_server);
        // {fileid++}
        writeMeta(MetaType::current_id);

        // std::ofstream ofs("DFSfiles/NameNode/meta");
        // if (ofs.is_open()) {
        //     std::time_t currentTime = std::time(nullptr);
        //     std::string timestamp = std::ctime(&currentTime);

        //     ofs << desFileName << " " << to_string(fileID) << " " << timestamp;
        //     // boost::archive::text_oarchive ov(ofs);
        //     // ov << desFileName << " " << to_string(fileID) << " " << timestamp << "\n";

        //     ofs.close();
        // } else {
        //     std::cerr << "Error: stat" << std::endl;
        // }

    }

// Assign read work to dataserver
bool NameServer::assignReadWork() const
    {
        // get the file info

        string logic_file_name;
        long long total_len = 0;

        // Form : read file_id offset count
        if (type == OperType::read)
        {
            auto file_info = fileid_path_lenMap.find(read_fileId);

            // Id doesn't exist
            if (file_info == fileid_path_lenMap.end())
            {
                cerr << "No such file with id = " << read_fileId << endl;
                return false;
            }
            
            // {fileid, {path, len}}
            else
            {
                logic_file_name = file_info->second.first;
                total_len = file_info->second.second;
            }
        }

        // Form : read path offset count
        else if (type == OperType::read2)
        {
            logic_file_name = read_filename;
            auto file_len = path_lenMap.find(read_filename);

            // File path doesn't exist
            if (file_len == path_lenMap.end())
            {
                cerr << "No such file with path = " << read_filename << endl;
                return false;
            }
            else{
                total_len = file_len->second;
            }
        }

        // Read size more than block size
        if (read_offset + read_count > total_len)
        {
            cerr << "Read failed: The read amount is greater than the maximum length of the data block"<< endl;
            return false;
        }

        // Calculate current block id to read
        int startblock = int(floor(read_offset / block_size));

        // Calculate space left for reading
        int spaceLeftOfThisBlock = int((startblock + 1) * block_size_int - read_offset);

        // Cannot read accoss blocks
        if (spaceLeftOfThisBlock < read_count)
        {
            cerr << "Read failed: Cannot read accoss blocks"<< endl;
            return false;
        }

        read_block = startblock;

        // Find the dataservers which contain the block
        auto server_id_iter = block_serversMap.find(logic_file_name + "-part" + to_string(startblock));
        // eg: {a.txt-part0, {0,1,2}} we can choose the first dataserver
        server_reading = server_id_iter->second[0];
        // Calculate the offset inside the block
        offset_in_block = int(read_offset - startblock * block_size_int);

        read_logic_file = logic_file_name;

        return true;
    }

// Assign fetch work
bool NameServer::assignFetchWork() const
    {

        if (type == OperType::fetch)
        {
            // Find the file for fetch
            // {fileid, {path, len}}
            auto fileInfo = fileid_path_lenMap.find(fetch_id);
            if (fileInfo == fileid_path_lenMap.end())
            {
                cerr << "No such file with id = " << fetch_id << endl;
                return false;
            }
            fetch_filepath = fileInfo->second.first;
        }

        // <logicFilePath, blockfiles>
        auto all_blocks = logicFile_BlockFileMap.find(fetch_filepath);

        if (all_blocks == logicFile_BlockFileMap.end())
        {
            cerr << "No such file with path = " << fetch_filepath << endl;
            return false;
        }

        // Block numbers
        fetch_blocks = all_blocks->second.size();

        // Traverse all blocks
        for (auto b : all_blocks->second)
        {
            
            auto part_pos = b.find_last_of("part");
            int blockID = stoi(b.substr(part_pos + 1));
            // {a.txt-part0, {0,1,2}}
            // Find all blocks on the given dataserver
            auto servers = block_serversMap.find(b);
            fetch_servers[blockID] = servers->second[0];

        }

        return true;
    }


// Read metadata
void NameServer::loadMeta() const
    {
        ifstream id_file_meta("DFSfiles/NameNode/id-logicpath-meta");
        // return if metadata doesn't exist
        if(!id_file_meta)
        {
            return;
        }

        boost::archive::text_iarchive id_file_ov(id_file_meta);
        id_file_ov >> fileid_path_lenMap;
        id_file_meta.close();

        ifstream file_block_meta("DFSfiles/NameNode/logicpath-blocks-meta");
        boost::archive::text_iarchive file_block_ov(file_block_meta);
        file_block_ov >> logicFile_BlockFileMap;
        file_block_meta.close();

        ifstream file_len_meta("DFSfiles/NameNode/logicpath-len-meta");
        boost::archive::text_iarchive file_len_ov(file_len_meta);
        file_len_ov >> path_lenMap;
        file_len_meta.close();

        ifstream block_server_meta("DFSfiles/NameNode/block-servers-meta");
        boost::archive::text_iarchive block_server_ov(block_server_meta);
        block_server_ov >> block_serversMap;
        block_server_meta.close();

        ifstream currentId_meta("DFSfiles/NameNode/current-id-meta");
        string currenetId;
        getline(currentId_meta, currenetId);
        fileID = stoi(currenetId);
        currentId_meta.close();

    }

// Write metadata
void NameServer::writeMeta(MetaType type) const
    {
        if (type == MetaType::id_file)
        {
            std::ofstream metaOfs("DFSfiles/NameNode/id-logicpath-meta");
            boost::archive::text_oarchive ov(metaOfs);
            ov << fileid_path_lenMap;
            metaOfs.close();

            std::ofstream ofs("DFSfiles/NameNode/meta", std::ios::app);
            // std::ofstream dir("DFSfiles/NameNode/dir", std::ios::app);
            if (ofs.is_open()) {
                std::time_t currentTime = std::time(nullptr);
                std::string timestamp = std::ctime(&currentTime);

                // ofs << "first: " << to_string(fileid_path_lenMap.end()->first) << " second: " << fileid_path_lenMap.end()->second.first << " " << timestamp;

                auto it = fileid_path_lenMap.begin();
                auto it_final = fileid_path_lenMap.begin();
                for (it = fileid_path_lenMap.begin(); it != fileid_path_lenMap.end(); it++){
                    // cout << it->first << " " << it->second.first << endl;
                    it_final = it;
                }

                // cout << "loop finished" << endl;
                // cout << it_final->first << " " << it_final->second.first << endl;
                ofs << "File id: " << it_final->first << "\tName: " << it_final->second.first << "\tFilesize: " << it_final->second.second << "\tTime: " << timestamp;
                // dir << it_final->second.first;
                filenameSet.insert(it_final->second.first);

                // cout << "write complete" << endl;

                ofs.close();
                // dir.close();
            } else {
                std::cerr << "Error: stat" << std::endl;
            }

        }
        else if (type == MetaType::file_len) {
            std::ofstream metaOfs("DFSfiles/NameNode/logicpath-len-meta");
            boost::archive::text_oarchive ov(metaOfs);
            ov << path_lenMap;
            metaOfs.close();
        }
        else if (type == MetaType::file_block)
        {
            std::ofstream metaOfs("DFSfiles/NameNode/logicpath-blocks-meta");
            boost::archive::text_oarchive ov(metaOfs);
            ov << logicFile_BlockFileMap;
            metaOfs.close();
        }
        else if (type == MetaType::block_server)
        {
            std::ofstream metaOfs("DFSfiles/NameNode/block-servers-meta");
            boost::archive::text_oarchive ov(metaOfs);
            ov << block_serversMap;
            metaOfs.close();

            // std::ofstream ofs("DFSfiles/NameNode/block", std::ios::app);
            std::ofstream ofs("DFSfiles/NameNode/block");
            if (ofs.is_open()) {

                auto it = block_serversMap.begin();
                for (it = block_serversMap.begin(); it != block_serversMap.end(); it++){
                    // // cout << it->first << " " << it->second.first << endl;
                    // it_final = it;

                    std::vector<int> vec = it->second;
                    string datanode = "";
                    for (auto& element : vec) {
                        datanode += "DataNode";
                        datanode += to_string(element);
                        datanode += " ";
                    }
                    ofs << "" << it->first << "\t" << datanode << "\n";

                }

                // cout << "loop finished" << endl;
                // cout << it_final->first << " " << it_final->second.first << endl;

                // ofs << "" << it_final->first << "\t" << it_final->second;

                ofs.close();

            } else {
                std::cerr << "Error: stat" << std::endl;
            }

        }
        else if (type == MetaType::current_id)
        {
            std::ofstream metaOfs("DFSfiles/NameNode/current-id-meta");
            metaOfs << to_string(fileID);
            metaOfs.close();
        }
    }