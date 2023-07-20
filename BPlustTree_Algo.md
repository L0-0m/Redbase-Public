
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