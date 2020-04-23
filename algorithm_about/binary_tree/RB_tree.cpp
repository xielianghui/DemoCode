#include <iostream>
#include <stack>
#include <queue>
using namespace std;

enum COLOR{
    RED,
    BLACK,
};

struct Node
{
    int val;
    Node* l;
    Node* r;
    Node* p;
    COLOR color;
    Node(int v, COLOR c = COLOR::RED) :val(v), l(NULL), r(NULL),p(NULL),color(c) {}
};

void LevelOrder(Node* root)
{
    std::queue<Node*> q;
    int size = 1;
    while (root != NULL || !q.empty())
    {
        if (root != NULL) {
            std::cout << root->val <<"("<<root->color<<")"<< " ";
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


void LL(Node*& root)
{
    Node* tmp = root->l;
    root->l = tmp->r;
    if(tmp->r != NULL){
        tmp->r->p = root;
    }
    tmp->r = root;
    tmp->p = root->p;
    root->p = tmp;
    root = tmp;
}

void RR(Node*& root)
{
    Node* tmp = root->r;
    root->r = tmp->l;
    if(tmp->l != NULL){
        tmp->l->p = root;
    }
    tmp->l = root;
    tmp->p = root->p;
    root->p = tmp;
    root = tmp;
}

void LR(Node*& root)
{
    RR(root->l);
    LL(root);
}

void RL(Node*& root)
{
    LL(root->r);
    RR(root);
}

void RebuildCon(Node*& root, Node* son, Node* parent)
{
    if(parent == NULL){
        root = son;
    }
    else if(parent->val > son->val){
        parent->l = son;
    }
    else{
        parent->r = son;
    }
}

void AddFixUp(Node*& root, Node* newNode)
{
    Node* p = NULL;
    Node* gp = NULL;
    while((p = newNode->p) && p->color == COLOR::RED)// 父节点存在且为红色
    {
        gp = p->p;
        // 如果插入节点的父节点是祖父节点的左子节点
        if(p == gp->l){
            Node* u = gp->r;
            // 叔父节点存在且为红色,只需要向上递归变色即可
            if(u != NULL && u->color == COLOR::RED){
                p->color = COLOR::BLACK;
                u->color = COLOR::BLACK;
                gp->color = COLOR::RED;
                newNode = gp;
                continue;
            }
            // 叔父节点不存在或者为黑色
            else{
                // 新节点是父节点的左孩子
                gp->color = COLOR::RED;
                if(newNode == p->l){
                    p->color = COLOR::BLACK;
                    LL(gp);
                }
                else{
                    newNode->color = COLOR::BLACK;
                    LR(gp);
                }
            }
        }
        // 如果插入节点的父节点是祖父节点的右子节点
        else{
            Node* u = gp->l;
            // 叔父节点存在且为红色,只需要向上递归变色即可
            if(u != NULL && u->color == COLOR::RED){
                p->color = COLOR::BLACK;
                u->color = COLOR::BLACK;
                gp->color = COLOR::RED;
                newNode = gp;
                continue;
            }
            // 叔父节点不存在或者为黑色
            else{
                gp->color = COLOR::RED;
                // 新节点是父节点的右孩子
                if(newNode == p->r){
                    p->color = COLOR::BLACK;
                    RR(gp);
                }
                else{
                    newNode->color = COLOR::BLACK;
                    RL(gp);
                }
            }
        }
        RebuildCon(root, gp, gp->p);
    }
    root->color = COLOR::BLACK;
}

void AddNode(Node*& root, int val)
{
    Node* newNode = new Node(val);
    Node* pre = root;
    Node* p = root;
    while(p != NULL)
    {
        pre = p;
        if(val > p->val){
            p = p->r;
        }
        else if(val < p->val){
            p = p->l;
        }
        else{
            return;
        }
    }
    newNode->p = pre;
    if(root != NULL){
        if(val > pre->val){
            pre->r = newNode;
        }
        else{
            pre->l = newNode;
        }
    }
    else{
        root = newNode;
    }
    AddFixUp(root, newNode);
}

void DelFixUp(Node*& root, Node* replace, Node* parent)
{
    // 进入这个函数说明删除节点一定是黑色
    if(replace != NULL){
        // 删除节点不是叶子节点时,那么删除节点的子节点一定是红色,将其变色即可
        replace->color = COLOR::BLACK;
    }
    else{
        // 删除节点是叶子节点时,因为删除节点为黑，所以兄弟节点一定存在
        Node* brother = NULL;
        while(replace != root)
        {
            if(parent->l == replace){
                // 删除节点是父节点的左子节点
                brother = parent->r;
                // 删除节点无侄子节点
                if(brother->l == NULL && brother->r == NULL){
                    if(parent->color == COLOR::RED){
                        parent->color = COLOR::BLACK;
                        brother->color = COLOR::RED;
                        break;
                    }
                    else{
                        brother->color = COLOR::RED;
                        replace = parent;
                        parent = replace->p; 
                    }
                }
                else{
                    // 删除节点有侄子节点
                    if(brother->color == COLOR::RED){
                        brother->color = COLOR::BLACK;
                        parent->color = COLOR::RED;
                        RR(parent);
                        RebuildCon(root, parent, parent->p);
                    }
                    else{
                        // 兄弟节点为黑色
                        if(brother->l != NULL){
                            brother->l->color = parent->color;
                            parent->color = COLOR::BLACK;
                            brother->l->color = COLOR::BLACK;
                            RL(parent);
                        }
                        else{
                            brother->color = parent->color;
                            parent->color = COLOR::BLACK;
                            brother->r->color = COLOR::BLACK;
                            RR(parent);
                        }
                        RebuildCon(root, parent, parent->p);
                        break;
                    }
                }
            }
            else{
                // 删除节点是父节点的右子节点
                brother = parent->l;
                // 删除节点无侄子节点
                if(brother->l == NULL && brother->r == NULL){
                    if(parent->color == COLOR::RED){
                        parent->color = COLOR::BLACK;
                        brother->color = COLOR::RED;
                        break;
                    }
                    else{
                        brother->color = COLOR::RED;
                        replace = parent;
                        parent = replace->p; 
                    }
                }
                else{
                    // 删除节点有侄子节点
                    if(brother->color == COLOR::RED){
                        brother->color = COLOR::BLACK;
                        parent->color = COLOR::RED;
                        LL(parent);
                        RebuildCon(root, parent, parent->p);
                    }
                    else{
                        // 兄弟节点为黑色
                        if(brother->r != NULL){
                            brother->l->color = parent->color;
                            parent->color = COLOR::BLACK;
                            brother->r->color = COLOR::BLACK;
                            LR(parent);
                        }
                        else{
                            brother->color = parent->color;
                            parent->color = COLOR::BLACK;
                            brother->l->color = COLOR::BLACK;
                            LL(parent);
                        }
                        RebuildCon(root, parent, parent->p);
                        break;
                    }
                }
            }
        }
    }
}

void DelNode(Node*& root, int val)
{
    Node* delNode = root;
    while(delNode != NULL)
    {
        if(val > delNode->val){
            delNode = delNode->r;
        }
        else if(val < delNode->val){
            delNode = delNode->l;
        }
        else{
            break;
        }
    }
    if(delNode == NULL) return;// 未找到删除节点
    //删除节点左右子树都存在
    if(delNode->l != NULL && delNode->r !=NULL){
        // 找到后继节点
        Node* hj = delNode->l;
        for(;hj->r != NULL;hj = hj->r);
        delNode->val = hj->val;
        delNode = hj;
    }
    //删除节点只有左/右子节点或者为叶子节点
    if(delNode->l == NULL || delNode->r ==NULL){
        // 删除节点为root节点
        if(delNode->p == NULL){
            root = (delNode->l == NULL ? delNode->r : delNode->l);
            if(root){
                root->p = NULL;
                root->color = COLOR::BLACK;
            }
            delete(delNode);
        }
        else{
            Node* delChild = (delNode->l != NULL ? delNode->l : delNode->r);
            if(delNode->p->l == delNode){
                delNode->p->l = delChild;
            }
            else{
                delNode->p->r = delChild;
            }
            if(delChild) delChild->p = delNode->p;
            if(delNode->color == COLOR::BLACK){
                DelFixUp(root, delChild, delNode->p);
            }
            delete delNode;
        }
    }
}

int main()
{
    int opt = 0;
    int addVal = 0;
    int delVal = 0;
    Node* root = NULL;
    /*Node* p1 = new Node(21,COLOR::BLACK);
    Node* p2 = new Node(23,COLOR::BLACK);
    Node* p3 = new Node(22,COLOR::RED);
    Node* p4 = new Node(12,COLOR::BLACK);
    p1->r = p2;
    p2->p = p1;
    p1->p = NULL;
    p2->l = p3;
    p3->p = p2;
    p1->l = p4;
    p4->p = p1;
    DelNode(p1, 12);
    LevelOrder(p1);*/
    while(true)
    {
        std::cout<<"-----1.ADD 2.DEL------"<<std::endl;
        std::cin>>opt;
        if(opt == 1){
            std::cin>>addVal;
            std::cout<<"----------------------"<<std::endl;
            AddNode(root, addVal);
            LevelOrder(root);
        }
        else if(opt == 2){
            std::cin>>delVal;
            std::cout<<"----------------------"<<std::endl;
            DelNode(root, delVal);
            LevelOrder(root);
        }
        else{
            std::cout<<"ERR OPT"<<std::endl;
        }
    }
    getchar();
    return 0;
}
