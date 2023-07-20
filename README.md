# Redbase
Stanford CS346 Redbase Project  
ref：https://web.stanford.edu/class/cs346/2015/

## 运行

* 运行环境：
  - OS：   Ubuntu 20.04.4 LTS X64
  - Linux：5.15.2-051502-generic
  - 编译器：g++ 9.4.0
  
* 构建&调试：
  - 打开终端并切换到`...redbase/src/`目录，输入`make demo_bplustree`可构建可执行文件`demo_bplustree`。可通过输入`gdb ./demo_bplustree`使用gdb进行调试。
  - 同理可根据test文件名，构建PF、RM、IX模块的测试文件。如`make ix_test`构建测试文件。
  - 输入`make clean`清除所有构建产生的文件。

## Rrcord Manager Component

* 简介
  - 为上层提供接口来管理文件中无序的record。

* 特点
  - record通过文件中唯一标识符`(PageNo, SlotNo)`索引
  - 页内record依靠bitmap索引，查找bitmap利用循环展开加速
  - 用链表组织文件内有空余slot的页面，方便插入

## Indexing Manager Component

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

```

# 删除

delete(key)
begin
    balanceNode = NO_BALANCE
    root = findRebalance(rootNode, NO_NODE, NO_NODE, NO_NODE, NO_NODE, key)
end



findRebalance(thisNode, leftNode, rightNode, lAnchor, rAnchor, key)
begin
    var removeNode, nextNode, nextLeft, nextRight, nextAncL, nextAncR

    // PART1:recursive descent from root to leaf node find the nodes needing rebalancing
    
    if thisNode is not minimal sized
        balanceNode = NO_BLLANCE
    else if balanceNode == NO_BALANCE
        balanceNode = currentNode

    // node location best matching key value
    nextNode = entry pointer for key
    
    if thisNode is not a leaf    // continue search
        
        // calculate neighbor & anchor nodes
        if nextNode is least entry in thisNode
            nextLeft = greatest entry pointer of leftNode
            nextAncL = lAnchor
        else
            nextLeft = preceding entry pointer
            nextAncL = thisNode

        if nextNode is greatest entry in thisNode
            nextRight = least entry pointer of rightMode
            nextAncR = rAnchor
        else
            nextRight = following entry pointer
            nextAncR = thisMode

        // recursive call
        removeNode = findRebalance(nextNode, nextLeft, nextRight, nextAncL, nextAncR, key)

    else    // key was found or not
        if entry pointer for key exists
            removeNode = nextNode
        else
            removeNode = NO_NODE

    //PART2:deletekey, unvindrecursion, rebalancetree, remove entry from current node
    
    if removeNode == nextNode
        clear removeNode entry in thisNode
        free removeNode memory
    
    // check which rebalancing actions are needed
    if balanceNode == NO_BALANCE
        done = NO_NODE
    else if thisNode is root
        done = collapseRoot(thisNode)
    else
        done = rebalance(thisNode, leftNode, rightNode, lAnchor, rAnchor)

    return done
end



collapseRoot(oldRoot)
begin
    if old Root is leaf
        newRoot = NO_NODE   //tree is empty
    else
        newRoot = entry pointer to root's solo child
    free old Root memory

    return newRoot
end



rebalance(thisNode, leftNode, rightNode, lAnchor, rAnchor)
begin
    
    // find a neighbor & anchor for rebalancing
    balanceNode = more full of { leftNode, rightNode }

    // select shift or merge operation
    if size(balenceNode) is not minimal
        anchorNode = balanceNode anchor in { lAnchor, rAnchor }
        done = shift(thisNode, balanceNode, anchorNode)
    else
        // at least one anchor is thisNode's parent
        anchorNode = thisNode parent in { lAnchor, rAnchor }
        mergeNode = anchorNode child in { leftNode, rightNode }
        done = merge(thisNode, mergeNode, anchorNode)
    
    return done
end



shift(thisNode, neighborNode, anchorNode)
begin
    // reference value separates the nodes' data
    if thisNode is an internal node
        copy anchorNode separator value to thisNode

    // equalize the nodes' sizes
    repeat
        shift neighborNode entries to this Node 
    until size(neighborNode) == size(thisNode)

    // new reference value reflects shifted data
    copy new separator value to anchorNode

    // no more nodes need removal
    balanceNode = NO_BALANCE
    
    return  NO_NODE
end



merge(thisNode, neighborNode, anchorNode)
begin
    
    // reference value separates the nodes' data
    if thisNode is an internal node
        copy anchorNode separator value to neighborNode //是 copy
    
    // empty one of the two nodes
    repeat
        shift thisNode entries to neighborNode 
    until size(thisNode) == 0

    // adjust node pointer value in leaf node
    if thisNode is leaf
        set thisNode's extra pointer to be neighborNode's

    // set empty node up for later removal
    return thisNode

end
```


