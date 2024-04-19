#include <bits/stdc++.h>
#include "coarsegrained.h"
using namespace std;

// Coarse-grained: IMPL=1, fine-grained: IMPL=2, lock-free: IMPL=3
#define IMPL 1
#define NUM_THREADS 8
#define THREAD_SIZE 1000

AVLTreeCG *treeCG;

/* UTILITY FUNCTIONS */
void initTree() {
    if (IMPL==1) treeCG = new AVLTreeCG();
}

void deleteTree() {
    if (IMPL==1) delete treeCG;
}

std::vector<int> getShuffledVector(int low, int high) {
    std::vector<int> v(high-low);
    std::iota(v.begin(), v.end(), low);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(v.begin(), v.end(), g);
    return v;
}

bool flexInsert(int k) {
    if (IMPL==1) return treeCG->insert(k);
}

bool flexDelete(int k) {
    if (IMPL==1) return treeCG->deleteNode(k);
}

bool flexSearch(int k) {
    if (IMPL==1) return treeCG->search(k);
}

/* HELPER FUNCTIONS */
void insertRange(int low, int high) {
    for (int i=low; i<high; i++) {
        flexInsert(i);
    }
}

void deleteRangeContiguous(int low, int high) {
	for (int i=low; i<high; i++) {
		flexDelete(i);
	}
}

void deleteRangeSpread(int low, int high) {
    for (int i=low; i<high; i+=2) {
        flexDelete(i);
    }
}

void insertRangeDeleteContiguous(int low, int high, int interval) {
    for (int i=low; i<high; i+=interval) {
        insertRange(i, min(high, i+interval));
        deleteRangeContiguous(i, min(high, i+interval));
    }
}

void insertRangeDeleteSpread(int low, int high, int interval) {
    for (int i=low; i<high; i+=interval) {
        insertRange(i, min(high, i+interval));
        deleteRangeSpread(i, min(high, i+interval));
    }
}

int checkHeightAndBalanceCG(NodeCG* node) {
    if (node==nullptr) return 0;
    int leftHeight = checkHeightAndBalanceCG(node->left);
    int rightHeight = checkHeightAndBalanceCG(node->right);
    if (node->height != 1+std::max(leftHeight, rightHeight))
        throw std::runtime_error("Node height is incorrect");
    int balance = leftHeight-rightHeight;
    if (balance<-1 || balance>1)
        throw std::runtime_error("Node is unbalanced");
    if (node->left && node->left->key>=node->key)
        throw std::runtime_error("Left child key is greater or equal to node key");
    if (node->right && node->right->key<=node->key)
        throw std::runtime_error("Right child key is lesser or equal to node key");
    if (node->left==node || node->right==node)
        throw std::runtime_error("Circular reference detected");
    return node->height;
}

int checkHeightAndBalance() {
    if (IMPL==1) {
        return checkHeightAndBalanceCG(treeCG->root);
    }
}

/* TEST FUNCTIONS */
void testSequentialSearch() {
    initTree();
    if (IMPL==1) {
        treeCG->root=new NodeCG(20);
        NodeCG* treeRoot = treeCG->root;
        treeRoot->left=new NodeCG(12);
        treeRoot->right=new NodeCG(53);
        treeRoot->left->left=new NodeCG(0);
        treeRoot->right->left=new NodeCG(21);
        treeRoot->left->right=new NodeCG(17);
        treeRoot->right->right=new NodeCG(82);
        treeRoot->right->right->left=new NodeCG(73);
        treeRoot->left->right->left=new NodeCG(15);
        treeRoot->left->left->right=new NodeCG(2);
    }
    std::set<int> elems = {20,12,53,0,21,17,82,73,15,2};
    for (int i=0; i<100; i++) {
        bool found = flexSearch(i);
        if (elems.find(i)!=elems.end() && !found) {
            std::ostringstream oss;
            oss << "Search failed, missing " << i << "\n";
            throw std::runtime_error(oss.str());
        } else if (elems.find(i)==elems.end() && found) {
            std::ostringstream oss;
            oss << "Search failed, incorrectly finding " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
    }
    printf("Sequential search passed!\n");
}

void testSequentialInsert() {
    initTree();
    insertRange(500, 900);
    insertRange(0, 100);
    insertRange(900, 1000);
    insertRange(100, 500);
    for (int i=0; i<1000; i++) {
        if (!flexSearch(i)) {
            std::ostringstream oss;
            oss << "Sequential insert failed, missing " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
    }
    checkHeightAndBalance();
    deleteTree();
    printf("Sequential insert passed!\n");
}

void testConcurrentInsert() {
    initTree();
    std::vector<std::thread> threads;
    for (int i=0; i<NUM_THREADS; i++) {
        threads.push_back(thread(insertRange, i*100, (i+1)*100));
    }
    for (int i=0; i<NUM_THREADS; i++) {
        threads[i].join();
    }
    for (int i=0; i<NUM_THREADS*100; i++) {
        if (!flexSearch(i)) {
            std::ostringstream oss;
            oss << "Concurrent insert failed, missing " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
    }
    checkHeightAndBalance();
    deleteTree();
    printf("Concurrent insert passed!\n");
}

void testSequentialDelete() {
    initTree();
    insertRange(0, THREAD_SIZE);
    deleteRangeContiguous(THREAD_SIZE/2, THREAD_SIZE);
    for (int i=0; i<THREAD_SIZE; i++) {
        bool found = flexSearch(i);
        if (i<THREAD_SIZE/2 && !found) {
            std::ostringstream oss;
            oss << "Sequential delete failed, missing " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
        else if (i>=THREAD_SIZE/2 && found) {
            std::ostringstream oss;
            oss << "Sequential delete failed, incorrectly finding " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
    }
    checkHeightAndBalance();
    deleteTree();

    initTree();
    insertRange(0, THREAD_SIZE);
    deleteRangeSpread(0, THREAD_SIZE);
    for (int i=0; i<THREAD_SIZE; i++) {
        bool found = flexSearch(i);
        if (i%2==1 && !found) {
            std::ostringstream oss;
            oss << "Sequential delete failed, missing " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
        else if (i%2==0 && found) {
            std::ostringstream oss;
            oss << "Sequential delete failed, incorrectly finding " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
    }
    checkHeightAndBalance();
    deleteTree();
    printf("Sequential delete passed!\n");
}

void testConcurrentDelete() {
	initTree();
	insertRange(0, NUM_THREADS * THREAD_SIZE);
	std::vector<std::thread> threads;
	for (int i = 0; i < NUM_THREADS; i++) {
		threads.push_back(thread(deleteRangeContiguous, i*THREAD_SIZE+THREAD_SIZE/4, (i+1)*THREAD_SIZE));
	}
	for (int i = 0; i<NUM_THREADS; i++) {
		threads[i].join();
	}
	for (int i = 0; i<NUM_THREADS * THREAD_SIZE; i++) {
        bool found = flexSearch(i);
		if (i%THREAD_SIZE<THREAD_SIZE/4 && !found) {
            std::ostringstream oss;
            oss << "Concurrent delete failed, missing " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
		else if (i%THREAD_SIZE>=THREAD_SIZE/4 && found) {
            std::ostringstream oss;
            oss << "Concurrent delete failed, incorrectly finding " << i << "\n";
            throw std::runtime_error(oss.str());
        } 
	}
    checkHeightAndBalance();
	deleteTree();
	printf("Concurrent deletion passed!\n");
}

void testInsertDeleteContiguous() {
	initTree();
	std::vector<std::thread> threads;
	for (int i=0; i<NUM_THREADS; i++) {
		threads.push_back(thread(insertRangeDeleteContiguous, i*THREAD_SIZE, (i+1)*THREAD_SIZE, (THREAD_SIZE+1)/2));
	}
	for (int i=0; i<NUM_THREADS; i++) {
		threads[i].join();
	}
	for (int i=0; i<NUM_THREADS*THREAD_SIZE; i++) {
		if (flexSearch(i)) {
            std::ostringstream oss;
            oss << "Insert/delete contiguous mix failed, incorrectly finding " << i << "\n";
            throw std::runtime_error(oss.str());
        }
	}
    checkHeightAndBalance();
	deleteTree();
	printf("insert delete test passed!\n");
}

void testInsertDeleteSpread() {
	initTree();
	std::vector<std::thread> threads;
	for (int i=0; i<NUM_THREADS; i++) {
		threads.push_back(thread(insertRangeDeleteSpread, i*THREAD_SIZE, (i+1)*THREAD_SIZE, (THREAD_SIZE+1)/2));
	}
	for (int i=0; i<NUM_THREADS; i++) {
		threads[i].join();
	}
	for (int i=0; i<NUM_THREADS*THREAD_SIZE; i++) {
        bool found = flexSearch(i);
        if (i%2==1 && !found) {
            std::ostringstream oss;
            oss << "Insert/delete spread mix failed, missing " << i << "\n";
            throw std::runtime_error(oss.str());
        }
        else if (i%2==0 && found) {
            std::ostringstream oss;
            oss << "Insert/delete spread mix failed, incorrectly finding " << i << "\n";
            throw std::runtime_error(oss.str());
        }
    }
    checkHeightAndBalance();
	deleteTree();
	printf("insert delete test passed!\n");
}

/* MAIN FUNCTION */
int main() {
    testSequentialSearch();
	testSequentialInsert();
	testSequentialDelete();
	for (int i=0; i<10; i++) {
		testConcurrentInsert();
	}
	for (int i=0; i<10; i++) {
		testConcurrentDelete();
	}
	for (int i=0; i<10; i++) {
		testInsertDeleteContiguous();
	}
	for (int i=0; i<10; i++) {
		testInsertDeleteSpread();
	}
}
