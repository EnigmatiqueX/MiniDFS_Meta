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


#include "Name_server.h"
#include "Data_server.h"


using namespace std;

int main(int argc, char* argv[])
{

    dataserver_num = 4;
    // start the thread of NameServer
    NameServer nameServer;
    thread ns(nameServer);

    // Create two folders for two servers
    boost::filesystem::path dfsPath{"DFSfiles"};
    boost::filesystem::create_directory(dfsPath);

    boost::filesystem::path namePath{"DFSfiles/NameNode"};
    boost::filesystem::create_directory(namePath);

    for (int i = 0; i < dataserver_num; i++)
    {
        // Create four folders
        string nodepath = "DFSfiles/DataNode " + to_string(i);
        boost::filesystem::path node{nodepath};
        boost::filesystem::create_directory(node);

        // Start four threads of dataservers
        DataServer dataServer(i);
        thread ds(dataServer);
        ds.detach();
    }

    ns.detach();

    string cmd;
    cout << "Welcome to MiniDFS!" << endl;
    cout << "MiniDFS> ";
    cout.flush();
    while (getline(cin, cmd))
    {
        // Check if the command is right
        isTrueCmd = getCmd(cmd);

        // NameNode starts to work
        {
            lock_guard<mutex> lk(cn_m);
            nameNotified = true;
        }
        // Notify thread of nameserver
        cn_cv.notify_all();

        unique_lock<mutex> cn_lk(cn_m);
        // wait for thread of nameserver finiished
        cn_cv.wait(cn_lk, []{return finish;});
        cn_lk.unlock();

        // Clear info
        server_fileRangesMap.clear();

        // Put
        if (type == OperType::put || type == OperType::put2)
        {
            if (isTrueCmd)
            {
                cout << "Upload suceesful ! File id = " << fileID - 1 << endl;
            }
            else
            {
                cerr << "Failed to upload file, check your command" << endl;
            }

            if (ifs.is_open())
            {
                ifs.close();
            }
        }
        // Fetch
        else if (is_ready_fetch && (type == OperType::fetch || type == OperType::fetch2))
        {
            boost::filesystem::remove(fetch_savepath);

            ofstream saveFile(fetch_savepath, ios_base::out | ios_base::app);

            for (int i = 0; i < fetch_blocks; i++)
            {


                ifstream blockFile;
                int serverID = fetch_servers[i];
                string blockFilePath = "DFSfiles/DataNode " +
                        to_string(serverID) + "/" + fetch_filepath + "-part" + to_string(i);
                blockFile.open(blockFilePath, ios::in);

                saveFile << blockFile.rdbuf();

                if(blockFile.is_open())
                {
                    blockFile.close();
                }
            }

            if (saveFile.is_open())
            {
                saveFile.close();
            }

            cout << "finish download!" << endl;
        }

        finish = false;

        cout << "MiniDFS> ";
        cout.flush();
    }


}