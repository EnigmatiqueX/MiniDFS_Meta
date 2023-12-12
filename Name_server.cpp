#include "Name_server.h"

using namespace std;

void NameServer::operator() () const
    {
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
            else if (isTrueCmd && type == OperType::read)
            {
                // assign read work
                is_ready_read = assignReadWork();
            }
            else if (isTrueCmd && type == OperType::mkdir)
            {
                tree->insert(mkdir_path, false);
            }
            else if (isTrueCmd && (type == OperType::fetch || type == OperType::fetch2))
            {
                is_ready_fetch = assignFetchWork();
            }

            nameNotified = false;

            fill_n(dataNotified.begin(), dataserver_num, true);

            nd_cv.notify_all();

            unique_lock<mutex> nd_lk(nd_m);
            nd_cv.wait(nd_lk, []{
                return all_of(dataNotified.begin(),
                              dataNotified.begin() + dataserver_num,
                              [](bool b){return !b;});
            });
            nd_lk.unlock();

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

        // Update fileId
        fileID++;

        // Store block path 
        vector<string> blockPathList;
        for (int i = 0; i < blockNum; i++)
        {
            blockPathList.push_back(desFileName + "-part" + to_string(i));
        }
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
        // {fileid, path, len}
        writeMeta(MetaType::id_file);
        // {path, len}
        writeMeta(MetaType::file_len);
        // {desFileName, blockPathList}
        writeMeta(MetaType::file_block);
        // {desFileName + "-part" + to_string(i), _serverids}
        writeMeta(MetaType::block_server);
        // {fileid++}
        writeMeta(MetaType::current_id);

    }

bool NameServer::assignReadWork() const
    {
        // get the file info

        string logic_file_name;
        long long total_len = 0;

        if (type == OperType::read)
        {
            auto file_info = fileid_path_lenMap.find(read_fileId);
            if (file_info == fileid_path_lenMap.end())
            {
                cerr << "No such file with id = " << read_fileId << endl;
                return false;
            }
            else
            {
                logic_file_name = file_info->second.first;
                total_len = file_info->second.second;
            }
        }
        else if (type == OperType::read2)
        {
            logic_file_name = read_filename;
            auto file_len = path_lenMap.find(read_filename);
            if (file_len == path_lenMap.end())
            {
                cerr << "No such file with path = " << read_filename << endl;
                return false;
            }
            else{
                total_len = file_len->second;
            }
        }


        if (read_offset + read_count > total_len)
        {
            cerr << "The expected reading exceeds"<< endl;
            return false;
        }

        int startblock = int(floor(read_offset / block_size));

        int spaceLeftOfThisBlock = int((startblock + 1) * block_size_int - read_offset);

        if (spaceLeftOfThisBlock < read_count)
        {
            // we assume that cannot read accoss blocks
            cerr << "Cannot read accoss blocks"<< endl;
            return false;
        }

        read_block = startblock;

        auto server_id_ite = block_serversMap.find(logic_file_name + "-part" + to_string(startblock));

        server_executing_read = server_id_ite->second[0];

        offset_in_block = int(read_offset - startblock * block_size_int);

        read_logic_file = logic_file_name;

        return true;
    }

bool NameServer::assignFetchWork() const
    {

        if (type == OperType::fetch)
        {
            auto fileInfo = fileid_path_lenMap.find(fetch_id);
            if (fileInfo == fileid_path_lenMap.end())
            {
                cerr << "No such file with id = " << fetch_id << endl;
                return false;
            }
            fetch_filepath = fileInfo->second.first;
        }

        auto all_blocks = logicFile_BlockFileMap.find(fetch_filepath);

        if (all_blocks == logicFile_BlockFileMap.end())
        {
            cerr << "No such file with path = " << fetch_filepath << endl;
            return false;
        }

        fetch_blocks = all_blocks->second.size();

                // block_serversMap
        for (auto b : all_blocks->second)
        {
            auto part_pos = b.find_last_of("part");
            int blockID = stoi(b.substr(part_pos + 1));
            auto servers = block_serversMap.find(b);
            fetch_servers[blockID] = servers->second[0];

        }

        return true;
    }

// Read metadata
void NameServer::loadMeta() const
    {
        ifstream id_file_meta("DFSfiles/NameNode/id-logicpath-meta");
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
        }
        else if (type == MetaType::current_id)
        {
            std::ofstream metaOfs("DFSfiles/NameNode/current-id-meta");
            metaOfs << to_string(fileID);
            metaOfs.close();
        }
    }