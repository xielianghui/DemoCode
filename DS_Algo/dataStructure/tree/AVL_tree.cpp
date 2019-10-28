#include <iostream>
#include <stack>
#include <queue>
using namespace std;

struct Node
{
    int val;
    Node* l;
    Node* r;
    Node(int v) :val(v), l(NULL), r(NULL) {}
};

int GetHeight(Node* root)
{
    if (root == NULL) return 0;
    int lHeight = GetHeight(root->l);
    int rHeight = GetHeight(root->r);
    return lHeight > rHeight ? (lHeight + 1) : (rHeight + 1);
}

void LL(Node** root)
{
    Node* leftNode = (*root)->l;
    (*root)->l = leftNode->r;
    leftNode->r = (*root);
    (*root) = leftNode;
}
void RR(Node** root)
{
    Node* rightNode = (*root)->r;
    (*root)->r = rightNode->l;
    rightNode->l = (*root);
    (*root) = rightNode;
}
void LR(Node** root)
{
    RR(&(*root)->l);
    LL(root);
}
void RL(Node** root)
{
    LL(&(*root)->r);
    RR(root);
}

void AddNode(Node** root, int val)
{
    Node* newNode = new Node(val);
    if (root == NULL || (*root) == NULL){
        (*root) = newNode;
        return;
    }
    std::stack<Node**> pathNode;
    while ((*root) != NULL)
    {
        pathNode.push(root);
        if (val < (*root)->val) {
            root = &((*root)->l);
        }
        else {
            root = &((*root)->r);
        }
    }
    if (val > (*pathNode.top())->val) {
        (*pathNode.top())->r = newNode;
    }
    else {
        (*pathNode.top())->l = newNode;
    }
    pathNode.pop();
    //adjust to fit
    while (!pathNode.empty())
    {
        Node** pos = pathNode.top();
        int lh = GetHeight((*pos)->l);
        int rh = GetHeight((*pos)->r);
        if (lh - rh > 1) {
            if (val < (*pos)->l->val) {//ll 单旋转
                LL(pos);
            }
            else {// LR双旋转
                LR(pos);
            }
        }
        else if (rh - lh > 1) {
            if (val > (*pos)->r->val) {//RR 单旋转
                RR(pos);
            }
            else {// RL双旋转
                RL(pos);
            }
        }
        pathNode.pop();
    }
}

Node* AddNode_re(Node* root, int val)
{
    if(root == NULL){
        return new Node(val);
    }
    else if(val > root->val){
        root->r = AddNode_re(root->r, val);
        if(GetHeight(root->r) - GetHeight(root->l) > 1){
            if(val > root->r->val){
                RR(&root);
            }else{
                RL(&root);
            }
        }
    }
    else if(val < root->val){
        root->l = AddNode_re(root->l, val);
        if(GetHeight(root->l) - GetHeight(root->r) > 1){
            if(val < root->l->val){
                LL(&root);
            }else{
                LR(&root);
            }
        }
    }
    return root;
}

void DelNode(Node** root, int val)
{
    if (root == NULL || (*root) == NULL) return;
    std::stack<Node**> pathNode;
    Node** delNode = NULL;
    while ((*root) != NULL)
    {
        pathNode.push(root);
        if (val < (*root)->val) {
            root = &((*root)->l);
        }
        else if (val > (*root)->val) {
            root = &((*root)->r);
        }
        else {
            delNode = root;
            break;
        }
    }
    if (delNode == NULL) return;
    if ((*delNode)->l == NULL) {//左子树为空
        Node* tmp = (*delNode);
        (*delNode) = (*delNode)->r;
        delete tmp;
        pathNode.pop();
    }
    else if ((*delNode)->r == NULL) {//右子树为空
        Node* tmp = (*delNode);
        (*delNode) = (*delNode)->l;
        delete tmp;
        pathNode.pop();
    }
    else {//左右子树都不为空
        Node* ltmp = (*delNode)->l;
        if (ltmp->r == NULL) {
            Node* tmp = (*delNode);
            (*delNode) = ltmp;
            (*delNode)->r = tmp->r;
            delete tmp;
        }
        else {
            Node* pre = ltmp;
            while (ltmp->r != NULL)
            {
                pre = ltmp;
                ltmp = ltmp->r;
            }
            pre->r = ltmp->l;
            Node* tmp = (*delNode);
            (*delNode) = ltmp;
            (*delNode)->l = tmp->l;
            (*delNode)->r = tmp->r;
            delete tmp;
            
        }
    }
    //adjust to fit
    while (!pathNode.empty())
    {
        Node** pos = pathNode.top();
        int lh = GetHeight((*pos)->l);
        int rh = GetHeight((*pos)->r);
        if (lh - rh > 1) {
            Node* ltmp = (*pos)->l;
            if (ltmp->l != NULL) {//ll 单旋转
                LL(pos);
            }
            else {// LR双旋转
                LR(pos);
            }
        }
        else if (rh - lh > 1) {
            Node* rtmp = (*pos)->r;
            if (rtmp->r != NULL) {//RR 单旋转
                RR(pos);
            }
            else {// RL双旋转
                RL(pos);
            }
        }
        pathNode.pop();
    }
}

Node* DelNode_re(Node* root, int val)
{
    if (root == NULL){
        return NULL;
    }
    else if (val > root->val){
        root->r = DelNode_re(root->r, val);
        if (GetHeight(root->l) - GetHeight(root->r) > 1){
            Node* tmp = root->l;
            if (tmp->l != NULL){
                LL(&root);
            }
            else{
                LR(&root);
            }
        }
    }
    else if (val < root->val){
        root->l = DelNode_re(root->l, val);
        if (GetHeight(root->r) - GetHeight(root->l) > 1){
            Node* tmp = root->r;
            if (tmp->r != NULL){
                RR(&root);
            }
            else{
                RL(&root);
            }
        }
    }
    else{
        if (root->l == NULL){ //左子树为空
            Node* tmp = root;
            root = root->r;
            delete tmp;
        }
        else if (root->r == NULL){ //右子树为空
            Node* tmp = root;
            root = root->l;
            delete tmp;
        }
        else{ //左右子树都不为空
            if(root->l->r == NULL){
                Node* tmp = root;
                root = root->l;
                root->r = tmp->r;
                delete tmp;
            }
            else{
                Node* p = root->l;
                Node* pre = root;
                while(p->r != NULL)
                {
                    pre = p;
                    p = p->r;
                }
                pre->r = p->l;
                p->l = root->l;
                p->r = root->r;
                delete root;
                root = p;
            }
        }
    }
    return root;
}

void InOrder(Node* root)
{
    std::queue<Node*> q;
    int size = 1;
    while (root != NULL || !q.empty())
    {
        if (root != NULL) {
            std::cout << root->val << " ";
            q.push(root->l);
            q.push(root->r);
        }
        --size;
        if (size == 0) {
            std::cout << std::endl;
            size = q.size();
        }
        root = q.front();
        q.pop();
    }
}

int main()
{
    Node* root = NULL;
    root = AddNode_re(root, 20);
    root = AddNode_re(root, 10);
    root = AddNode_re(root, 30);
    root = AddNode_re(root, 40);
    root = AddNode_re(root, 25);
    root = AddNode_re(root, 9);
    root = AddNode_re(root, 11);
    root = AddNode_re(root, 8);
    //root = AddNode_re(root, 27);
    InOrder(root);
    std::cout << std::endl;
    //root = DelNode_re(root, 30);
    DelNode(&root,30);
    InOrder(root);
    getchar();
    return 0;
}
