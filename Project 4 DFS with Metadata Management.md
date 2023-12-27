# **Project 3/4 Distributed File System with Metadata Management**

## 1. 背景与功能介绍：

本项目旨在设计一个**小型文件系统（Mini-DFS）**，其中包括：

- A Name Server
- A Client
- Four Data Servers

![image-20231215161018607](images\image-1)

其中Mini-DFS是通过一个**进程**来运行的。 在这个过程中，Name Server和Data Servers是**不同的线程**。通过多线程实现，模拟分布式环境下TCP通信环境。根据图中示意，我们使用1个线程作为Name Server，维护Mini-DFS的元数据信息和任务调度；使用4个线程作为Data Server，负责文件的存储。

Mini-DFS实现的基本功能如下所示：

- **读/写文件**

  - 上传文件：上传成功并返回文件ID
  - 根据文件ID和偏移量读取文件的位置

- **文件分割**

  -  将文件分割成多个块

  - 每个块为 2MB

  -  这些块在四个数据服务器之间均匀分布

- **数据块复制**
  -  每个块有三个复制
  -  副本分布在不同的数据服务器中
- **MD5校验**
  - 对不同Data Server中的块使用 MD5 校验，结果相同
- **目录管理**
  - 在给定目录中写入文件
  - 通过“目录+文件名”访问文件

其中每个模块有自己的具体任务：

- **Name Server**
  -  列出文件和块之间的关系
  - 列出副本和Data Server之间的关系
  -  Data Server管理
- **Data Server**
  - 读/写本地块
  - 通过本地目录路径写入块
- **Client**
  - 提供文件的读/写接口

## 2. 编译与运行

```
// github下载
git clone https://github.com/EnigmatiqueX/MiniDFS_Meta.git
// 创建build目录并且执行CMakeLists.txt文件, 再make生成可执行文件
mkdir build
cd build
cmake ..
make
// 执行
./DFS
```

这样我们就启动了启动文件系统，然后我们就可以输入mini-DFS命令来进行使用。目前实现的主要命令如下：

```
put：
# 将本地文件上传到mini-DFS
MiniDFS > put source_file_path
# 将本地文件上传到指定目标路径的mini-DFS
MiniDFS > put2 source_file_path des_file_path

read：
# 读取mini-DFS上的文件：文件ID，偏移量，长度
MiniDFS > read file_id offset count
# 读取mini-DFS上的文件：文件路径，偏移量，长度
MiniDFS > read2 file_path offset count

fetch：
# 下载mini-DFS上的文件：文件ID，保存路径
MiniDFS > fetch file_id save_path
# 下载mini-DFS上的文件：文件路径，保存路径
MiniDFS > fetch2 file_path save_path

mkdir：
# 在mini-DFS上创建目录
MiniDFS > mkdir mkdir_path

cd:
# 进入mini-DFS指定目录
MiniDFS > cd cd_path

ls:
# 查看mini-DFS指定目录下所有文件
MiniDFS > ls ls_path

locate:
# 查看文件是否在mini-DFS的指定目录下
MiniDFS > locate locate_path

quit:
# 退出
MiniDFS > quit
```



## 3. 项目模块介绍

本项目主要包含主函数模块，解析指令模块，Name  Server模块， Data  Server模块和File tree五个模块

![image-2](images\image-2.png)

### 3.1  主函数模块

​	main函数的功能主要有以下几点（给出部分代码示例）：

1. 初始化建立Name Server和Data Server文件夹存放元数据信息以及数据块

   ```c++
   	// Create two folders for two servers
       boost::filesystem::path dfsPath{"DFSfiles"};
       boost::filesystem::create_directory(dfsPath);
   
       boost::filesystem::path namePath{"DFSfiles/NameNode"};
       boost::filesystem::create_directory(namePath);
   ```

   

2. 创建Name Server和4个Data Server线程

   ```c++
   	// start the thread of NameServer
       NameServer nameServer;
       thread ns(nameServer);
   
   	// Start four threads of dataservers
       DataServer dataServer(i);
       thread ds(dataServer);
       ds.detach();
   ```

   

3. 创建锁和条件变量用于main函数（client）和Name Server之间的线程通信

   ```c++
   // NameNode starts to work
   {
       lock_guard<mutex> lk(cn_m);
       nameNotified = true;
   }
   // Notify thread of nameserver
   cn_cv.notify_all();
   
   unique_lock<mutex> cn_lk(cn_m);
   // wait for thread of nameserver finished
   cn_cv.wait(cn_lk, []{return finish;});
   cn_lk.unlock();
   ```

   

4. 打印当前位置信息，根据命令解析的结果输出相对应的信息

   ```c++
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
   ```

### 3.2  解析命令模块

​	本模块的主要功能如下所示（给出部分代码示例）：

1. 声明并且定义全局变量，Name Server类和Data  Server类中的函数需要使用的变量

   ```c++
   // 2MB
   constexpr double block_size = 2.0 * 1024 * 1024;
   
   constexpr int block_size_int = 2 * 1024 * 1024;
   
   // Four dataservers and three replications for each chunk
   int dataserver_num = 4;
   
   int replicate_num = 3;
   
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
   ```

   

2. 通过对输入的命令进行解析，将命令中包含的信息赋予相对应的变量

   ```c++
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
   ```

   

3. 将创建的文件夹和写入的文件插入File tree当中

   ```c++
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
   ```

   

### 3.3  Name  Server模块

Name Server模块主要功能有以下几点：

1. 加载上一条命令执行之后的metadata

   ```c++
   // load metadata before
     loadMeta();
   ```

   

2. 根据命令解析的结果执行对应的功能

   ```c++
   // Store block info in NameNode
   if (isTrueCmd && (type == OperType::put || type == OperType::put2))
   {
        StoreBlockInfo();
   }
   ```

3. 创建锁和条件变量用于和Data Server的thread进行通信

   ```c++
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
   ```

4. 存储文件分割之后每个文件块对应的信息，并将信息写入metadata之中，用于Data Server的put执行

   ```c++
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
       
       	// For more details, see the entire code
           ...
      }
   ```

5. 分配给Data Server对应的文件块读取任务，用于Data Server的read执行

   ```c++
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
           
           // For more details, see the entire code
           ...
       }
   ```

6. 分配给Data Server对应的文件块下载任务，用于Data Server的fetch执行

   ```c++
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
           
           // For more details, see the entire code
           ...
       }
   ```

7. 执行file tree的函数来完成信息输出

   ```c++
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
   ```

### 3.4  Data  Server模块

Data Server模块主要功能如下所示：

1. 根据命令解析的结果执行对应的功能

   ```c++
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
   ```

2. 将文件块保存到对应的路径下

   ```c++
   void DataServer::saveFile() const
       {
           // DFSfiles/DataNode[0,1,2,3]/
           // check the server_fileRangesMap
           // <serverid, range{start, count, blockid}>
           // key = serverId
           auto fileRanges = server_fileRangesMap.equal_range(serverId);
           string nodePath = "DFSfiles/DataNode " + to_string(serverId);
   
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
   ```

3. 读取对应路径下的文件块

   ```c++
   void DataServer::readFileAndOutput() const
       {
           string blockpath = "DFSfiles/DataNode " + to_string(serverId) + "/"
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
   ```

4. 创建文件夹

   ```c++
   // Create folder
   void DataServer::mkdir() const
       {
           string folder = "DFSfiles/DataNode " + to_string(serverId) + "/" + mkdir_path;
           boost::filesystem::create_directories(folder);
       }
   ```

   

### 3.5  File tree模块

File tree模块主要包含以下功能：

1. 定义split函数对输入的命令行进行解析，用于后续对file tree进行操作

   ```c++
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
   ```

2. 初始化file tree包含根目录

   ```c++
   // Initialize FileTree
   FileTree::FileTree()
       {
           root = new TreeNode("/", false);
       }
   ```

3. 定义find函数，用于查询指定路径的文件夹或文件是否存在于file tree，同时更新当前目录

   ```c++
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
   ```

4. 定义insert函数，用于将指定路径的文件夹或文件插入file tree

   ```c++
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
   ```

5. 定义cd函数，用于进入指定路径的文件夹

   ```c++
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
   ```

6. 定义ls函数，用于查看指定路径的文件夹下的所有文件

   ```c++
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
   ```

7. 定义locate函数，用于查看文件是否存在于指定路径

   ```c++
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
   ```

## 4. 结果展示

1. 启动Mini-DFS：

![image-3](images\image-3.png)

![image-4](images\image-4.png)

2.  将指定本地文件上传：**put source_file_path**

![image-5](images\image-5.png)

我们可以看到文件a.txt被分为了五个部分并且均匀分布在每个Data Server上

![image-6](images\image-6.png)

3. 创建文件夹：**mkdir mkdir_path**

![image-7](images\image-7.png)

在每个Data Server中会创建hello文件夹，同时被添加到file tree中

![image-8](images\image-8.png)

4. 将指定本地文件上传到指定路径：**put2 source_file_path des_file_path**

![image-9](images\image-9.png)

我们可以看到文件a.txt被分为了五个部分并且均匀分布在每个Data Server的五个hello文件夹中

![image-10](images\image-10.png)

5. 读取mini-DFS上的文件：文件ID，偏移量，长度 **read file_id offset count**

![image-11](images\image-11.png)

6. 读取mini-DFS上的文件：文件路径，偏移量，长度 **read2 file_path offset count**

![image-12](images\image-12.png)

7. 下载mini-DFS上的文件：文件ID，保存路径 **fetch file_id save_path**

![image-13](images\image-13.png)

可以看到file id为0的文件已经被下载到了d2.txt中

![image-14](images\image-14.png)

8. 下载mini-DFS上的文件：文件路径，保存路径 **fetch2 file_path save_path**

![image-15](images\image-15.png)

可以看到file path为hello/d1.txt的文件已经被下载到了d3.txt中

![image-16](images\image-16.png)

9. 进入mini-DFS指定目录 **cd_path**

![image-17](images\image-17.png)

10. 查看mini-DFS指定目录下所有文件 **ls_path**

![image-18](images\image-18.png)

11. 查看文件是否在mini-DFS的指定目录下 **locate locate_path**

![image-19](images\image-19.png)

12. **MD5校验** 对不同Data Server中的块使用 MD5 校验，结果相同

    备份之间的校验：

    ![image-21](images\image-21.png)

    下载后的文件与原文件的校验：

    ![image-22](images\image-22.png)

13. 查看所有文件目录 **readdir**

    ![image-23](images\image23.png)

14. 查看所有文件元数据 **stat**

    ![image-24](images\image24.png)

15. 查看指定文件的具体存储位置 **check file_name**

    ![image-25](images\image25.png)

16. 退出Mini-DFS **quit**





