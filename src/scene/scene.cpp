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

void Node3D::_propogate_input_key(int key, int scancode, int action, int mods) {
	for (Node3D *child : _children) {
		child->_propogate_input_key(key, scancode, action, mods);
	}
	LOG_DEBUG(
			"input_key: name=%s key=%d scancode=%d action=%d mods=%d",
			name,
			key,
			scancode,
			action,
			mods);
	input_key(key, scancode, action, mods);
}
void Node3D::_propogate_input_char(unsigned int codepoint) {
	for (Node3D *child : _children) {
		child->_propogate_input_char(codepoint);
	}
	input_char(codepoint);
}
void Node3D::_propogate_input_cursor_pos(double x, double y) {
	for (Node3D *child : _children) {
		child->_propogate_input_cursor_pos(x, y);
	}
	input_cursor_pos(x, y);
}
void Node3D::_propogate_input_mouse_button(int button, int action, int mods) {
	for (Node3D *child : _children) {
		child->_propogate_input_mouse_button(button, action, mods);
	}
	input_mouse_button(button, action, mods);
}

void Node3D::init() {}

void Node3D::update(float delta) {}

void Node3D::draw(DrawContext *context) {
	for (Node3D *child : _children) {
		context->draw(child);
	}
}

void Node3D::input_key(int key, int scancode, int action, int mods) {}

void Node3D::input_char(unsigned int codepoint) {}

void Node3D::input_cursor_pos(double x, double y) {}

void Node3D::input_mouse_button(int button, int action, int mods) {}

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

void Node3D::print_tree() const {
	_print_tree(0, false);
}

void Node3D::_print_tree(int depth, bool is_last) const {
	std::string indent = depth == 0 ? "" : " ";
	for (int i = 1; i < depth; i++)
		indent += "| ";

	LOG_DEBUG(
			"%s%s%s",
			indent.c_str(),
			depth == 0 ? "" : (is_last ? "└ " : "├ "),
			name);

	const auto child_count = _children.size();
	for (int i = 0; i < child_count; i++)
		_children[i]->_print_tree(depth + 1, child_count - 1 == i);
}