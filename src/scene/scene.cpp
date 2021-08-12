#include "scene.h"

using namespace Opal;

void Node3D::_set_tree_root(RenderObject *tree_root) {
	_tree_root = (Node3D *)tree_root;

	// LOG_DEBUG("Node3D _is_in_tree = %s: %s", in_tree ? "true" : "false",
	// name);

	for (Node3D *child : _children) {
		child->_set_tree_root(_tree_root);
	}

	if (_tree_root != nullptr) {
		init();
	}
}

void Node3D::_propigate_update(float delta) {
	for (Node3D *child : _children) {
		child->_propigate_update(delta);
	}
	// LOG_DEBUG("delta: %f, name: %s", delta, name);
	update(delta);
}

void Node3D::init() {}

void Node3D::update(float delta) {}

void Node3D::draw(DrawContext *context) {
	for (Node3D *child : _children) {
		context->draw(child);
	}
}

bool Node3D::is_ancestor_of(Node3D *node) {

	// given node has no parent.
	if (node->_parent == nullptr)
		return false;

	// this node is parent of given node.
	if (node->_parent == this)
		return true;

	// recursively call up the tree.
	return node->_parent->is_ancestor_of(this);
}

void Node3D::add_child(Node3D *child) {

	ERR_FAIL_COND_MSG(child == nullptr, "child is null");
	ERR_FAIL_COND_MSG(
			child == this, "%s can't be added to itself", child->name);
	ERR_FAIL_COND_MSG(
			child->_parent != nullptr,
			"Node3D already has a parent. Remove it from the parent before "
			"calling add_child() on a new parent.");
	ERR_FAIL_COND_MSG(
			child->is_ancestor_of(this), "Child is a parent of this node");

	_children.push_back(child);
	child->_parent = this;

	// propigate tree state to children.
	// this calls init on the child and it's children if this node is in the
	// tree.
	child->_set_tree_root(_tree_root);
}

void Node3D::remove_child(Node3D *child) {
	ERR_FAIL_COND_MSG(child == nullptr, "child is null");

	auto ret = std::remove(_children.begin(), _children.end(), child);
}
