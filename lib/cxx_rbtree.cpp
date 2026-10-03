/**
 * @file lib/cxx_rbtree.cpp
 * @brief Red-black tree node algorithms backing std::map/std::set.
 *
 * <bits/stl_tree.h> declares a handful of non-template functions --
 * `_Rb_tree_increment`, `_Rb_tree_decrement`, `_Rb_tree_insert_and_rebalance`,
 * `_Rb_tree_rebalance_for_erase` -- and leaves them out-of-line: in a normal
 * install their bodies live in libstdc++'s compiled tree.cc. Everything else
 * about std::map/std::set is header-only templates (same as vector/string),
 * so this file is the one piece standing between "compiles" and "links".
 *
 * `_Rb_tree_increment` and `_Rb_tree_decrement` each come in *two* overloads,
 * taking `_Rb_tree_node_base *` and `const _Rb_tree_node_base *`, and both are
 * separately exported by libstdc++ (all four are GLIBCXX_3.4 symbols). The
 * const ones are what `_Rb_tree_const_iterator::operator++/--` call, so
 * defining only the non-const pair is enough for a container walked through a
 * mutable iterator but *not* for one walked through a const_iterator --
 * `cbegin()`, or a range-for over a `const` map. GCC 15's header happens to
 * route its const_iterator through the non-const overload and so never
 * references them, which is why leaving them out went unnoticed there while
 * GCC 13 and GCC 14 failed to link apps/hello-map and apps/hello-set.
 *
 * `_Rb_tree_node_base` (color + parent/left/right pointers) is a fixed,
 * public part of libstdc++'s ABI, included here via <map> rather than
 * redeclared, so the layout is guaranteed to match whatever this GCC's
 * headers actually generate. The tree algorithms themselves are the
 * standard, widely-published red-black tree insert-fixup /
 * delete-fixup / in-order successor-predecessor operations (the same shape
 * used by essentially every STL-compatible rb-tree implementation, since the
 * node layout and header-node convention -- `header._M_parent` = root,
 * `header._M_left` = leftmost, `header._M_right` = rightmost -- are fixed by
 * the ABI above) reimplemented here from scratch, not copied from GCC's own
 * tree.cc.
 */

#include <map>

namespace std {

using Node = _Rb_tree_node_base;

static void rotate_left(Node *const x, Node *&root) {
    Node *const y = x->_M_right;
    x->_M_right = y->_M_left;
    if (y->_M_left != 0) {
        y->_M_left->_M_parent = x;
    }
    y->_M_parent = x->_M_parent;

    if (x == root) {
        root = y;
    } else if (x == x->_M_parent->_M_left) {
        x->_M_parent->_M_left = y;
    } else {
        x->_M_parent->_M_right = y;
    }
    y->_M_left = x;
    x->_M_parent = y;
}

static void rotate_right(Node *const x, Node *&root) {
    Node *const y = x->_M_left;
    x->_M_left = y->_M_right;
    if (y->_M_right != 0) {
        y->_M_right->_M_parent = x;
    }
    y->_M_parent = x->_M_parent;

    if (x == root) {
        root = y;
    } else if (x == x->_M_parent->_M_right) {
        x->_M_parent->_M_right = y;
    } else {
        x->_M_parent->_M_left = y;
    }
    y->_M_right = x;
    x->_M_parent = y;
}

_Rb_tree_node_base *_Rb_tree_increment(_Rb_tree_node_base *x) throw() {
    if (x->_M_right != 0) {
        x = x->_M_right;
        while (x->_M_left != 0) {
            x = x->_M_left;
        }
    } else {
        Node *y = x->_M_parent;
        while (x == y->_M_right) {
            x = y;
            y = y->_M_parent;
        }
        if (x->_M_right != y) {
            x = y;
        }
    }
    return x;
}

/* `_Rb_tree_const_iterator::operator++` calls this overload, not the one
 * above -- so a map/set walked through a const_iterator (`cbegin()`, a
 * range-for over a `const` map) needs it to link, while one only ever walked
 * through a mutable iterator does not. Upstream casts the const away and
 * reuses the non-const traversal; so do we. That is sound here because the
 * walk only reads `_M_parent`/`_M_left`/`_M_right`: constness of the
 * *elements* is carried by the caller's iterator type, not by these links. */
const _Rb_tree_node_base *_Rb_tree_increment(const _Rb_tree_node_base *x) throw() {
    return _Rb_tree_increment(const_cast<_Rb_tree_node_base *>(x));
}

_Rb_tree_node_base *_Rb_tree_decrement(_Rb_tree_node_base *x) throw() {
    if (x->_M_color == _S_red && x->_M_parent->_M_parent == x) {
        /* x is the header node (root's parent); "predecessor of end()" is
         * the tree's maximum, cached at header._M_right. */
        x = x->_M_right;
    } else if (x->_M_left != 0) {
        Node *y = x->_M_left;
        while (y->_M_right != 0) {
            y = y->_M_right;
        }
        x = y;
    } else {
        Node *y = x->_M_parent;
        while (x == y->_M_left) {
            x = y;
            y = y->_M_parent;
        }
        x = y;
    }
    return x;
}

/* Same story as the const `_Rb_tree_increment` above, for
 * `_Rb_tree_const_iterator::operator--`. */
const _Rb_tree_node_base *_Rb_tree_decrement(const _Rb_tree_node_base *x) throw() {
    return _Rb_tree_decrement(const_cast<_Rb_tree_node_base *>(x));
}

void _Rb_tree_insert_and_rebalance(const bool insert_left, _Rb_tree_node_base *x,
                                    _Rb_tree_node_base *p, _Rb_tree_node_base &header) throw() {
    Node *&root = header._M_parent;

    x->_M_parent = p;
    x->_M_left = 0;
    x->_M_right = 0;
    x->_M_color = _S_red;

    if (insert_left) {
        p->_M_left = x;
        if (p == &header) {
            header._M_parent = x;
            header._M_right = x;
        } else if (p == header._M_left) {
            header._M_left = x;
        }
    } else {
        p->_M_right = x;
        if (p == header._M_right) {
            header._M_right = x;
        }
    }

    while (x != root && x->_M_parent->_M_color == _S_red) {
        Node *const parent = x->_M_parent;
        Node *const grandparent = parent->_M_parent;

        if (parent == grandparent->_M_left) {
            Node *const uncle = grandparent->_M_right;
            if (uncle != 0 && uncle->_M_color == _S_red) {
                parent->_M_color = _S_black;
                uncle->_M_color = _S_black;
                grandparent->_M_color = _S_red;
                x = grandparent;
            } else {
                if (x == parent->_M_right) {
                    x = parent;
                    rotate_left(x, root);
                }
                x->_M_parent->_M_color = _S_black;
                grandparent->_M_color = _S_red;
                rotate_right(grandparent, root);
            }
        } else {
            Node *const uncle = grandparent->_M_left;
            if (uncle != 0 && uncle->_M_color == _S_red) {
                parent->_M_color = _S_black;
                uncle->_M_color = _S_black;
                grandparent->_M_color = _S_red;
                x = grandparent;
            } else {
                if (x == parent->_M_left) {
                    x = parent;
                    rotate_right(x, root);
                }
                x->_M_parent->_M_color = _S_black;
                grandparent->_M_color = _S_red;
                rotate_left(grandparent, root);
            }
        }
    }
    root->_M_color = _S_black;
}

_Rb_tree_node_base *_Rb_tree_rebalance_for_erase(_Rb_tree_node_base *const z, _Rb_tree_node_base &header) throw() {
    Node *&root = header._M_parent;
    Node *&leftmost = header._M_left;
    Node *&rightmost = header._M_right;
    Node *y = z;
    Node *x = 0;
    Node *x_parent = 0;

    if (y->_M_left == 0) {
        x = y->_M_right;
    } else if (y->_M_right == 0) {
        x = y->_M_left;
    } else {
        /* z has two children: y becomes z's in-order successor, which has
         * no left child, and x is that successor's (possibly null) right
         * child -- the node that takes the successor's place. */
        y = y->_M_right;
        while (y->_M_left != 0) {
            y = y->_M_left;
        }
        x = y->_M_right;
    }

    if (y != z) {
        /* Splice successor y into z's place before touching colors. */
        z->_M_left->_M_parent = y;
        y->_M_left = z->_M_left;
        if (y != z->_M_right) {
            x_parent = y->_M_parent;
            if (x != 0) {
                x->_M_parent = y->_M_parent;
            }
            y->_M_parent->_M_left = x;
            y->_M_right = z->_M_right;
            z->_M_right->_M_parent = y;
        } else {
            x_parent = y;
        }

        if (root == z) {
            root = y;
        } else if (z->_M_parent->_M_left == z) {
            z->_M_parent->_M_left = y;
        } else {
            z->_M_parent->_M_right = y;
        }
        y->_M_parent = z->_M_parent;
        _Rb_tree_color tmp = y->_M_color;
        y->_M_color = z->_M_color;
        z->_M_color = tmp;
        y = z;
        /* y now names the (relinked, off-tree) node actually being freed
         * by the caller; its color is z's original color, checked below. */
    } else {
        x_parent = y->_M_parent;
        if (x != 0) {
            x->_M_parent = y->_M_parent;
        }
        if (root == z) {
            root = x;
        } else if (z->_M_parent->_M_left == z) {
            z->_M_parent->_M_left = x;
        } else {
            z->_M_parent->_M_right = x;
        }

        if (leftmost == z) {
            leftmost = (z->_M_right == 0) ? z->_M_parent : Node::_S_minimum(x);
        }
        if (rightmost == z) {
            rightmost = (z->_M_left == 0) ? z->_M_parent : Node::_S_maximum(x);
        }
    }

    if (y->_M_color != _S_red) {
        while (x != root && (x == 0 || x->_M_color == _S_black)) {
            if (x == x_parent->_M_left) {
                Node *sibling = x_parent->_M_right;
                if (sibling->_M_color == _S_red) {
                    sibling->_M_color = _S_black;
                    x_parent->_M_color = _S_red;
                    rotate_left(x_parent, root);
                    sibling = x_parent->_M_right;
                }
                if ((sibling->_M_left == 0 || sibling->_M_left->_M_color == _S_black) &&
                    (sibling->_M_right == 0 || sibling->_M_right->_M_color == _S_black)) {
                    sibling->_M_color = _S_red;
                    x = x_parent;
                    x_parent = x_parent->_M_parent;
                } else {
                    if (sibling->_M_right == 0 || sibling->_M_right->_M_color == _S_black) {
                        if (sibling->_M_left != 0) {
                            sibling->_M_left->_M_color = _S_black;
                        }
                        sibling->_M_color = _S_red;
                        rotate_right(sibling, root);
                        sibling = x_parent->_M_right;
                    }
                    sibling->_M_color = x_parent->_M_color;
                    x_parent->_M_color = _S_black;
                    if (sibling->_M_right != 0) {
                        sibling->_M_right->_M_color = _S_black;
                    }
                    rotate_left(x_parent, root);
                    break;
                }
            } else {
                Node *sibling = x_parent->_M_left;
                if (sibling->_M_color == _S_red) {
                    sibling->_M_color = _S_black;
                    x_parent->_M_color = _S_red;
                    rotate_right(x_parent, root);
                    sibling = x_parent->_M_left;
                }
                if ((sibling->_M_right == 0 || sibling->_M_right->_M_color == _S_black) &&
                    (sibling->_M_left == 0 || sibling->_M_left->_M_color == _S_black)) {
                    sibling->_M_color = _S_red;
                    x = x_parent;
                    x_parent = x_parent->_M_parent;
                } else {
                    if (sibling->_M_left == 0 || sibling->_M_left->_M_color == _S_black) {
                        if (sibling->_M_right != 0) {
                            sibling->_M_right->_M_color = _S_black;
                        }
                        sibling->_M_color = _S_red;
                        rotate_left(sibling, root);
                        sibling = x_parent->_M_left;
                    }
                    sibling->_M_color = x_parent->_M_color;
                    x_parent->_M_color = _S_black;
                    if (sibling->_M_left != 0) {
                        sibling->_M_left->_M_color = _S_black;
                    }
                    rotate_right(x_parent, root);
                    break;
                }
            }
        }
        if (x != 0) {
            x->_M_color = _S_black;
        }
    }
    return y;
}

} // namespace std
