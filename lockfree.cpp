#include <lockfree.h>

#define FLAG_MASK 3UL
#define NULL_MASK 1UL

// Operation marking status
enum opFlag {
    NONE, MARK, ROTATE, INSERT, RELOCATE
};

Operation* AVLTreeLF::flag(Operation* op, int status) {
    return (Operation*) ((uintptr_t)op | status);
}

Operation* AVLTreeLF::getFlag(Operation* op) {
    return (Operation*) ((uintptr_t)op & FLAG_MASK);
}

Operation* AVLTreeLF::unflag(Operation* op) {
    return (Operation*)(((uintptr_t)op >> 2) << 2);
}

Operation* AVLTreeLF::setNull(Operation* op) {
    return (NodeLF*)((uintptr_t) node | NULL_MASK);
}

bool AVLTreeLF::search(int k, NodeLF* node) {
    bool result = false;
    while (node!=nullptr) {
        if (k<node->key)
            node = node->left;
        else if (k>node->key)
            node = node->right;
        else {
            result = true;
            break;
        }
    }
    return result;
}

int AVLTreeLF::seek(int key, NodeLF** parent, Operation** parentOp, NodeLF** node, Operation** nodeOp, NodeLF* auxRoot) {
    NodeLF* next;
    int result;
    bool retry = false;
    while (true) {
        result = -1;
        *node = auxRoot;
        *nodeOp = (*node)->op;
        if (getFlag(*nodeOp)!=NONE) {
            if (auxRoot==root) {
                helpInsert(unflag(*nodeOp), *node);
                continue;
            } else
                return -2;
        }
        next = (*node)->right;
        while (next!=nullptr) {
            *parent = *node
            *parentOp = *nodeOp;
            *node = next;
            *nodeOp = (*node)->op;
            if (getFlag(nodeOp) != NONE) {
                helpSeek(*parent, *parentOp, *node, *nodeOp);
                retry = true;
                break;
            }
            int nodeKey = (*node)->key;
            if (key<nodeKey) {
                next = (*node)->left;
                result = -1;
            } else if (key>nodeKey) {
                next = (*node)->right;
                result = 1;
            } else
                return 0;
        }
        if (!retry)
            return result;
        else
            retry = false;
    }
    return result;
}

bool AVLTreeLF::insert(int key) {
    NodeLF* parent;
    NodeLF* current;
    NodeLF* newNode = nullptr;
    Operation* parentOp;
    Operation* currentOp;
    Operation* casOp;
    int result = 0;
    while (true) {
        result = seek(key, &parent, &parentOp, &current, &currentOp, root);
        if (result==0)
            return false;
        if (newNode==nullptr)
            newNode = new NodeLF(key);
        bool isLeft = (result==-1);
        NodeLF* oldNode = isLeft ? current->left : current->right;
        casOp = new InsertOp(isLeft, oldNode, newNode);
        if (__sync_bool_compare_and_swap(&current->op, currentOp, flag(casOp, INSERT)) {
            helpInsert(casOp, current);
            return true;
        }
    }
}

bool AVLTreeLF::deleteNode(int key) {
    NodeLF* parent;
    NodeLF* node;
    NodeLF* newNode;
    Operation* parentOp;
    Operation* nodeOp;
    Operation* newNodeOp;
    while (true) {
        int result = seek(key, &parent, &parentOp, &node, &nodeOp, root);
        if (result!=0)
            return false;
        if (node->left==nullptr || node->right==nullptr) {
            if (__sync_bool_compare_and_swap(&node->op, nodeOp, SET_FLAG(nodeOp, MARK))) {
                helpMarked(parent, parentOp, node);
                return true;
            }
        }
        else {
            if (seek(key, parent, parentOp, newNode, newNodeOp, node) == -2 || (node->op!=nodeOp))
                continue;
            RelocateOp relocateOp = new RelocateOp(node, nodeOp, key, newNode->key);
            if (__sync_bool_compare_and_swap(&newNode->op, newNodeOp, flag(relocateOp, RELOCATE))) {
                if (helpRelocate(relocateOp, parent, parentOp, newNode))
                    return true;
            }
        }
    }
}

void AVLTreeLF::helpSeek(NodeLF* parent, Operation* parentOp, NodeLF* node, NodelF* nodeOp) {
    if (getFlag(nodeOp)==INSERT)
        helpInsert(unflag(nodeOp), node);
    else if (getFlag(parentOp)==ROTATE)
        helpRotate((unflag(parentOp), parent, node, parentOp->child);
    else if (getFlag(nodeOp)==MARK)
        helpMarked(parent, parentOp, node);
}

void AVLTreeLF::helpInsert(Operation* op, NodeLF* dest) {
    InsertOp* insertOp = (InsertOp *)op;
    volatile NodeLF** address = nullptr;
    if (insertOp->isLeft)
        address = (nodeLF**)&(dest->left);
    else
        address = (nodeLF**)&(dest->right);
    __sync_bool_compare_and_swap(address, insertOp->expected, insertOp->update);
    __sync_bool_compare_and_swap(&(dest->op), flag(op, INSERT), flag(op, NONE));
}

void helpMarked(NodeLF* parent, Operation* parentOp, NodeLF* node) {
    NodeLF* child, address;
    if (node->left==nullptr)
        if (node->right==nullptr)
            child = (NodeLF*)setNull(node);
        else
            child = node->right;
    else
        child = node->left;
    Operation* casOp = new InsertOp((node==parent->left), node, child);
    if (__sync_bool_compare_and_swap(&parent->op, parentOp, setFlag(casOp, INSERT)))
        helpInsert(casOp, parent);
    else
        delete casOp;
}

bool helpRelocate(RelocateOp* op, Node* pred, Operation* predOp, Node* curr) {
    int seenState = op->state;
    if (seenState == ONGOING) {
        Operation* seenOp = VCAS(&op->dest->op, op->destOp, FLAG(op, RELOCATE));
        if ((seenOp == op->destOp) || (seenOp == FLAG(op, RELOCATE))) {
            CAS(&op->state, ONGOING, SUCCESSFUL);
            seenState = SUCCESSFUL;
        } else {
            seenState = VCAS(&op->state, ONGOING, FAILED);
        }
    }
    if (seenState == SUCCESSFUL) {
        CAS(&op->dest->key, removeKey, replaceKey);
        CAS(&op->dest->op, FLAG(op, RELOCATE), FLAG(op, NONE));
    }
    bool result = (seenState == SUCCESSFUL);
    if (op->dest == curr)
        return result;
    CAS(&curr->op, FLAG(op, RELOCATE), FLAG(op, result ? MARK : NONE));
    if (result) {
    if (op->dest == pred) predOp = FLAG(op, NONE);
        helpMarked(pred, predOp, curr);
    }
    return result;
}
