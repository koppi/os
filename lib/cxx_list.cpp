/**
 * cxx_list.cpp -- NOT Qt source; a minimal freestanding
 * implementation of the two out-of-line std::list node-splice primitives
 * (real libstdc++ defines these in its compiled src/c++98/list.cc, which
 * this kernel doesn't link -- same category as lib/cxx_rbtree.cpp/
 * lib/cxx_hashtable.cpp for std::map/unordered_map, and this phase's own
 * cxx_pmr_koppios.cpp). Textbook doubly-linked-list insert-before/
 * remove, matching the real algorithm exactly (this is the one part of
 * <bits/stl_list.h> with no implementation-defined freedom -- the node
 * layout and semantics are fixed by the header itself).
 */
#include <list>

namespace std {
namespace __detail {

void _List_node_base::_M_hook(_List_node_base *const position) noexcept
{
    _M_next = position;
    _M_prev = position->_M_prev;
    position->_M_prev->_M_next = this;
    position->_M_prev = this;
}

void _List_node_base::_M_unhook() noexcept
{
    _List_node_base *const next = _M_next;
    _List_node_base *const prev = _M_prev;
    prev->_M_next = next;
    next->_M_prev = prev;
}

} // namespace __detail
} // namespace std
