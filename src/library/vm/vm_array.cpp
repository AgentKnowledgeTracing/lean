/*
Copyright (c) 2017 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <vector>
#include "library/parray.h"
#include "library/vm/vm.h"
#include "library/vm/vm_nat.h"

namespace lean {
struct vm_array : public vm_external {
    parray<vm_obj> m_array;
    vm_array(parray<vm_obj> const & a):m_array(a) {}
    virtual ~vm_array() {}
    virtual void dealloc() override { this->~vm_array(); get_vm_allocator().deallocate(sizeof(vm_array), this); }
    virtual vm_external * ts_clone(vm_clone_fn const &) override;
    virtual vm_external * clone(vm_clone_fn const &) override { lean_unreachable(); }
};

/* Auxiliary object used by vm_array::ts_clone.
   This is the "thread safe" version used when creating a ts_vm_obj that contains
   a nested vm_array. */
struct vm_array_ts_copy : public vm_external {
    std::vector<vm_obj> m_entries;
    virtual ~vm_array_ts_copy() {
        /* The object ts_vm_obj manages the life cycle of all vm_obj's.
           We should prevent this destructor from invoking the destructor of
           vm_obj's nested in m_entries. */
        for (auto & p : m_entries) {
            p.steal_ptr();
        }
    }
    virtual void dealloc() override { lean_unreachable(); }
    virtual vm_external * ts_clone(vm_clone_fn const &) override { lean_unreachable(); }
    virtual vm_external * clone(vm_clone_fn const &) override;
};

vm_external * vm_array::ts_clone(vm_clone_fn const & fn) {
    vm_array_ts_copy * r = new vm_array_ts_copy();
    size_t sz = m_array.size();
    for (size_t i = 0; i < sz; i++) {
        r->m_entries.emplace_back(fn(m_array[i]));
    }
    return r;
}

vm_external * vm_array_ts_copy::clone(vm_clone_fn const & fn) {
    parray<vm_obj> array;
    for (vm_obj const & p : m_entries) {
        array.push_back(fn(p));
    }
    return new (get_vm_allocator().allocate(sizeof(vm_array))) vm_array(array);
}

parray<vm_obj> const & to_array(vm_obj const & o) {
    lean_vm_check(dynamic_cast<vm_array*>(to_external(o)));
    return static_cast<vm_array*>(to_external(o))->m_array;
}

vm_obj to_obj(parray<vm_obj> const & a) {
    return mk_vm_external(new (get_vm_allocator().allocate(sizeof(vm_array))) vm_array(a));
}

vm_obj array_read(vm_obj const &, vm_obj const &, vm_obj const & a, vm_obj const & i) {
    /* TODO(Leo): handle case where n is too big */
    unsigned idx = force_to_unsigned(i);
    lean_vm_check(idx < to_array(a).size());
    parray<vm_obj> const & _a = to_array(a);
    return _a[idx];
}

vm_obj array_write(vm_obj const &, vm_obj const &, vm_obj const & a, vm_obj const & i, vm_obj const & v) {
    /* TODO(Leo): handle case where n is too big */
    unsigned idx = force_to_unsigned(i);
    lean_vm_check(idx < to_array(a).size());
    parray<vm_obj> _a = to_array(a);
    _a.set(idx, v);
    return to_obj(_a);
}

vm_obj array_push_back(vm_obj const &, vm_obj const &, vm_obj const & a, vm_obj const & v) {
    parray<vm_obj> _a = to_array(a);
    _a.push_back(v);
    return to_obj(_a);
}

vm_obj array_pop_back(vm_obj const &, vm_obj const &, vm_obj const & a) {
    parray<vm_obj> _a = to_array(a);
    lean_vm_check(_a.size() != 0);
    _a.pop_back();
    return to_obj(_a);
}

vm_obj mk_array(vm_obj const &, vm_obj const & n, vm_obj const & v) {
    /* TODO(Leo): handle case where n is too big */
    unsigned _n = force_to_unsigned(n);
    parray<vm_obj> a(_n, v);
    return to_obj(a);
}

vm_obj array_mk(vm_obj const &, vm_obj const & n, vm_obj const & fn) {
    /* TODO(Leo): handle case where n is too big */
    unsigned _n = force_to_unsigned(n);
    parray<vm_obj> a;
    for (unsigned i = 0; i < _n; ++i) {
        a.push_back(invoke(fn, mk_vm_nat(i)));
    }
    return to_obj(a);
}

static unsigned g_array_read_idx = -1;

unsigned array_cases_on(vm_obj const & o, buffer<vm_obj> & data) {
    vm_obj d[3] = {o, mk_vm_unit(), mk_vm_unit()};
    vm_obj fn = mk_vm_closure(g_array_read_idx, 3, d);
    data.push_back(fn);
    return 0;
}

void initialize_vm_array() {
    DECLARE_VM_BUILTIN(name({"array", "mk"}),               array_mk);
    DECLARE_VM_BUILTIN(name({"mk_array"}),                  mk_array);
    DECLARE_VM_BUILTIN(name({"array", "read"}),             array_read);
    DECLARE_VM_BUILTIN(name({"array", "write"}),            array_write);
    DECLARE_VM_BUILTIN(name({"array", "push_back"}),        array_push_back);
    DECLARE_VM_BUILTIN(name({"array", "pop_back"}),         array_pop_back);
    DECLARE_VM_CASES_BUILTIN(name({"array", "cases_on"}),   array_cases_on);
}

void finalize_vm_array() {
}

void initialize_vm_array_builtin_idxs() {
    g_array_read_idx = *get_vm_builtin_idx(name({"array", "read"}));
}
}
