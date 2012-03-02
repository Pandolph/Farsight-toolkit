#include "Tree.h"

Tree::Tree()
{
	this->root = NULL;
}

Tree::~Tree()
{
	//TODO: Delete all the nodes in the tree, otherwise big memory leak!
}

void Tree::SetRoot(Node *root)
{
	member_nodes.push_back(root);
	this->root = root;
}

void Tree::AddNode(Node* node, Node* parent)
{
	if (parent == NULL)
		throw std::runtime_error("AddNode needs a parent");

	member_nodes.push_back(node);
}

std::vector<Node*> Tree::GetMemberNodes()
{
	return member_nodes;
}

Node* Tree::getRoot()
{
	return root;
}