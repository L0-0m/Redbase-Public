# Redbase
A relational database implementation based on [Stanford CS346 Redbase Project](https://web.stanford.edu/class/cs346/2015) 

## 运行

* 运行环境：
  - OS:         Ubuntu 22.04.1 LTS X64
  - Linux:      5.19.0-46-generic
  - Compiler:   g++ 11.3.0
  
* 构建&调试：
  - 打开终端并切换到`src`目录，输入`make all | module_name`即可编译`所有模块`|`特定模块`。
  - 以B+Tree为例，如要测试可执行`make demo_bplustree`，调试可通过`gdb ./demo_bplustree`。
  - 其他模块详见Makefile文件，如测试 ix_模块，可执行`make ix_test`。
  - 输入`make clean`清除所有构建产生的文件。

## [The Paged File Component](https://web.stanford.edu/class/cs346/2015/redbase-pf.html)

* 简介
  - 一个存储与缓冲区管理器，负责管理Page的磁盘存储与缓冲区调度。
  - 设定连续的SIZE(=4096B)大小为一个Page，Page是数据在内存-磁盘之间交换的单位。
  - 在磁盘上以若干Page组成的链表式堆文件的形式物化(materialization)数据；
  - 在内存中维护一个 Buffer Pool 来缓存 Pages，并根据 LRU 策略调度缓存。

* 特点
  - 采用链表式堆文件形式组织磁盘数据。
  - 内存中维护 Buffer Pool ，通过 hashtable 实现命中查询，组织 LRU 链调度缓存数据。

## [Rrcord Manager Component](https://web.stanford.edu/class/cs346/2015/redbase-rm.html)

* 简介
  - 为上层提供接口来管理文件中无序的record。

* 特点
  - record通过文件中唯一标识符`(PageNo, SlotNo)`索引
  - 页内record依靠bitmap索引，查找bitmap利用循环展开加速
  - 用链表组织文件内有空余slot的页面，方便插入

## [Indexing Manager Component](https://web.stanford.edu/class/cs346/2015/redbase-ix.html)

* 简介
  - 为上层提供接口来为record中属性构建B+树索引。

* 特点
  - 完整实现B+树插入&删除算法（递归形式）
  - 支持在非主键上构建索引：B+树索引的leaf指向一个Bucket，可以存储相同键值的不同rid。

* node数据结构
   - `level`   ：标记当前Node层次。  
   - `keyNum`  ：当前Node上entry个数。  
   - `entraPtr`：若为Internal节点，则指向最小子树；若为leaf节点，则指向后一个邻居节点。  
   - `prevPtr` ：若为Internal节点，则不使用；若为leaf节点，则指向前一个邻居节点。  

```
|----------------------------|
|   pageNo   level    keyNum |
|   ---       ---      ---   |
|      key_0   |   ptr_0     |
|      key_1   |   ptr_1     |
|      key_2   |   ptr_2     |
|   ---       ---      ---   |
|   extraPtr       prevtPtr  |
|----------------------------|
 ```
* bucket数据结构
  - 作为leaf所指向的节点，用于存储rid。rid在bucket内部组织成list，freeList组织空闲rid位置，useList组织已使用的rid位置。  
  - 当前Bucket满后，会生成新的bucket，这些bucket之间组织成链表，通过双向指针`nextPtr`和`prevPtr`相互联系。

```
|----------------------------|
|   pageNo   level    ridNum |
|   freeList       useList   |
|   ---       ---      ---   |
|    | rid ,  nextRidPos|    |
|    | rid ,  nextRidPos|    |
|    | rid ,  nextRidPos|    |
|   ---       ---      ---   |
|   nextPtr        prevtPtr  |
|----------------------------|
```

* 删除算法
  - 算法参考：[B+ tree deletion algorithms](https://web.stanford.edu/class/cs346/2015/notes/jannink.pdf)
  - 项目中实现的 B+tree 伪代码详见：[伪代码](https://github.com/L0-0m/Redbase-Public/blob/main/BPlusTree_Algo.md)

## [The System Management Component](https://web.stanford.edu/class/cs346/2015/redbase-sm.html)

* 简介
  - 一个系统管理层，相当于粘合剂将前面三个模块统一管理。
  - 可以实现简单的数据库定义语句（如record的增删改查以及index的建立与删除等）。
  - 可以实现简单的系统管理语句（如建立数据库，删除数据库等。）
* 注意
    编译该模块依赖`flex`和`bison`环境。
