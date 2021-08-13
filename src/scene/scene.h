#ifndef __SCENE_H__
#define __SCENE_H__

#include <vector>

#include "../renderer/renderer.h"
#include "../renderer/vk_debug.h"
#include "../renderer/vk_types.h"

#include "../utils/log.h"

namespace Opal {

class Node3D : public RenderObject {

protected:
	Node3D *_tree_root;
	Node3D *_parent;
	std::vector<Node3D *> _children;

public:
	/**
	 * The 3D transform of this object.
	 */
	glm::mat4 transform;

	Node3D() : RenderObject("Node3D") {}
	Node3D(const char *name) : RenderObject(name) {}

	void _set_tree_root(RenderObject *tree_root);
	void _propigate_update(float delta);

	void _propogate_input_key(int key, int scancode, int action, int mods);
	void _propogate_input_char(unsigned int codepoint);
	void _propogate_input_cursor_pos(double x, double y);
	void _propogate_input_mouse_button(int button, int action, int mods);

	void init();
	void update(float delta);
	void draw(DrawContext *context);

	virtual void input_key(int key, int scancode, int action, int mods);
	virtual void input_char(unsigned int codepoint);
	virtual void input_cursor_pos(double x, double y);
	virtual void input_mouse_button(int button, int action, int mods);

	/**
	 * @returns true if this node is a parent of the given node somewhere up the
	 * tree.
	 */
	bool is_ancestor_of(Node3D *node);

	void add_child(Node3D *child);
	void remove_child(Node3D *child);

	void print_tree() const;

protected:
	void _print_tree(int depth, bool is_last) const;
};

} // namespace Opal
#endif // __SCENE_H__