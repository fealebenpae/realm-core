/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#include <algorithm>

#include <realm/link_view.hpp>
#include <realm/column_linklist.hpp>
#include <realm/replication.hpp>
#include <realm/table_view.hpp>

using namespace realm;

void LinkView::insert(size_t link_ndx, size_t target_row_ndx)
{
    do_insert(link_ndx, target_row_ndx);
    if (Replication* repl = get_repl())
        repl->link_list_insert(*this, link_ndx, target_row_ndx); // Throws
}


void LinkView::do_insert(size_t link_ndx, size_t target_row_ndx)
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT_7(m_row_indexes.is_attached(), ==, true, ||, link_ndx, ==, 0);
    REALM_ASSERT_7(m_row_indexes.is_attached(), ==, false, ||, link_ndx, <=, m_row_indexes.size());
    REALM_ASSERT_3(target_row_ndx, <, m_origin_column->get_target_table().size());
    typedef _impl::TableFriend tf;
    tf::bump_version(*m_origin_table);

    size_t origin_row_ndx = get_origin_row_index();

    // if there are no links yet, we have to create list
    if (!m_row_indexes.is_attached()) {
        REALM_ASSERT_3(link_ndx, ==, 0);
        ref_type ref = IntegerColumn::create(m_origin_column->get_alloc()); // Throws
        m_origin_column->set_row_ref(origin_row_ndx, ref);                  // Throws
        m_row_indexes.init_from_parent();                                   // re-attach
    }

    m_row_indexes.insert(link_ndx, target_row_ndx);                // Throws
    m_origin_column->add_backlink(target_row_ndx, origin_row_ndx); // Throws
}


void LinkView::set(size_t link_ndx, size_t target_row_ndx)
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT_7(m_row_indexes.is_attached(), ==, true, &&, link_ndx, <, m_row_indexes.size());
    REALM_ASSERT_3(target_row_ndx, <, m_origin_column->get_target_table().size());

    if (Replication* repl = get_repl())
        repl->link_list_set(*this, link_ndx, target_row_ndx); // Throws

    do_set(link_ndx, target_row_ndx); // Throws
}


// Replication instruction 'link-list-set' calls this function directly.
size_t LinkView::do_set(size_t link_ndx, size_t target_row_ndx)
{
    size_t old_target_row_ndx = to_size_t(m_row_indexes.get(link_ndx));
    size_t origin_row_ndx = get_origin_row_index();
    m_origin_column->remove_backlink(old_target_row_ndx, origin_row_ndx); // Throws
    m_origin_column->add_backlink(target_row_ndx, origin_row_ndx);        // Throws
    m_row_indexes.set(link_ndx, target_row_ndx);                          // Throws
    typedef _impl::TableFriend tf;
    tf::bump_version(*m_origin_table);
    return old_target_row_ndx;
}


void LinkView::move(size_t from_link_ndx, size_t to_link_ndx)
{
    if (REALM_UNLIKELY(!is_attached()))
        throw LogicError(LogicError::detached_accessor);
    if (REALM_UNLIKELY(!m_row_indexes.is_attached() || from_link_ndx >= m_row_indexes.size() ||
                       to_link_ndx >= m_row_indexes.size()))
        throw LogicError(LogicError::link_index_out_of_range);

    if (from_link_ndx == to_link_ndx)
        return;

    typedef _impl::TableFriend tf;
    tf::bump_version(*m_origin_table);

    // FIXME: Can get() return -1 now that we have detached entries? Does this move() work with it?
    size_t target_row_ndx = static_cast<size_t>(m_row_indexes.get(from_link_ndx));
    m_row_indexes.erase(from_link_ndx);
    m_row_indexes.insert(to_link_ndx, target_row_ndx);

    if (Replication* repl = get_repl())
        repl->link_list_move(*this, from_link_ndx, to_link_ndx); // Throws
}

void LinkView::swap(size_t link_ndx_1, size_t link_ndx_2)
{
    if (REALM_UNLIKELY(!is_attached()))
        throw LogicError(LogicError::detached_accessor);
    if (REALM_UNLIKELY(!m_row_indexes.is_attached() || link_ndx_1 >= m_row_indexes.size() ||
                       link_ndx_2 >= m_row_indexes.size()))
        throw LogicError(LogicError::link_index_out_of_range);

    // Internally, core requires that the first link index is strictly less than
    // the second one. The changeset merge mechanism is written to take
    // advantage of it, and requires it.
    if (link_ndx_1 == link_ndx_2)
        return;
    if (link_ndx_1 > link_ndx_2)
        std::swap(link_ndx_1, link_ndx_2);

    typedef _impl::TableFriend tf;
    tf::bump_version(*m_origin_table);

    size_t target_row_ndx = to_size_t(m_row_indexes.get(link_ndx_1));
    m_row_indexes.set(link_ndx_1, m_row_indexes.get(link_ndx_2));
    m_row_indexes.set(link_ndx_2, target_row_ndx);

    if (Replication* repl = get_repl())
        repl->link_list_swap(*this, link_ndx_1, link_ndx_2); // Throws
}


void LinkView::remove(size_t link_ndx)
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT_7(m_row_indexes.is_attached(), ==, true, &&, link_ndx, <, m_row_indexes.size());

    if (Replication* repl = get_repl())
        repl->link_list_erase(*this, link_ndx);  // Throws

    do_remove(link_ndx); // Throws
}


// Replication instruction 'link-list-erase' calls this function directly.
size_t LinkView::do_remove(size_t link_ndx)
{
    size_t target_row_ndx = to_size_t(m_row_indexes.get(link_ndx));
    size_t origin_row_ndx = get_origin_row_index();
    m_origin_column->remove_backlink(target_row_ndx, origin_row_ndx); // Throws
    m_row_indexes.erase(link_ndx);                                    // Throws
    typedef _impl::TableFriend tf;
    tf::bump_version(*m_origin_table);
    return target_row_ndx;
}


void LinkView::clear()
{
    // Now implemented in List<Key>::clear()
}


// Replication instruction 'link-list-clear' calls this function directly.
void LinkView::do_clear(bool broken_reciprocal_backlinks)
{
    size_t origin_row_ndx = get_origin_row_index();
    if (!broken_reciprocal_backlinks && m_row_indexes.is_attached()) {
        size_t num_links = m_row_indexes.size();
        for (size_t link_ndx = 0; link_ndx < num_links; ++link_ndx) {
            size_t target_row_ndx = to_size_t(m_row_indexes.get(link_ndx));
            m_origin_column->remove_backlink(target_row_ndx, origin_row_ndx); // Throws
        }
    }

    m_row_indexes.destroy();
    m_origin_column->set_row_ref(origin_row_ndx, 0); // Throws

    typedef _impl::TableFriend tf;
    tf::bump_version(*m_origin_table);
}


void LinkView::sort(size_t column_index, bool ascending)
{
    sort(SortDescriptor(m_origin_column->get_target_table(), {{column_index}}, {ascending}));
}


void LinkView::sort(SortDescriptor&& order)
{
    if (Replication* repl = get_repl()) {
        // todo, write to the replication log that we're doing a sort
        repl->set_link_list(*this, m_row_indexes); // Throws
    }
    DescriptorOrdering ordering;
    ordering.append_sort(std::move(order));
    do_sort(ordering);
}

void LinkView::do_nullify_link(size_t old_target_row_ndx)
{
    REALM_ASSERT(m_row_indexes.is_attached());

    size_t pos = m_row_indexes.find_first(old_target_row_ndx);
    REALM_ASSERT_3(pos, !=, realm::not_found);

    if (Replication* repl = m_origin_table->get_repl())
        repl->link_list_nullify(*this, pos);

    m_row_indexes.erase(pos);

    if (m_row_indexes.is_empty()) {
        m_row_indexes.destroy();
        size_t origin_row_ndx = get_origin_row_index();
        m_origin_column->set_row_ref(origin_row_ndx, 0);
    }
}


void LinkView::do_update_link(size_t old_target_row_ndx, size_t new_target_row_ndx)
{
    REALM_ASSERT(m_row_indexes.is_attached());

    // While there may be multiple links in this list pointing to the specified
    // old target row index, This function is supposed to only update the first
    // one. If there are more links pointing to the same target row, they will
    // be updated by subsequent involcations of this function. I.e., it is the
    // responsibility of the caller to call this function the right number of
    // times.
    size_t pos = m_row_indexes.find_first(old_target_row_ndx);
    REALM_ASSERT_3(pos, !=, realm::not_found);

    m_row_indexes.set(pos, new_target_row_ndx);
}

void LinkView::do_swap_link(size_t target_row_ndx_1, size_t target_row_ndx_2)
{
    REALM_ASSERT(m_row_indexes.is_attached());

    // FIXME: Optimize this.
    size_t len = m_row_indexes.size();
    for (size_t i = 0; i < len; ++i) {
        int64_t value_64 = m_row_indexes.get(i);
        REALM_ASSERT(!util::int_cast_has_overflow<size_t>(value_64));
        size_t value = size_t(value_64);

        if (value == target_row_ndx_1) {
            m_row_indexes.set(i, target_row_ndx_2);
        }
        else if (value == target_row_ndx_2) {
            m_row_indexes.set(i, target_row_ndx_1);
        }
    }
}


void LinkView::repl_unselect() noexcept
{
    if (Replication* repl = get_repl())
        repl->on_link_list_destroyed(*this);
}

uint_fast64_t LinkView::sync_if_needed() const
{
    if (m_origin_table)
        return m_origin_table->m_version;

    return std::numeric_limits<uint_fast64_t>::max();
}


#ifdef REALM_DEBUG

void LinkView::verify(size_t row_ndx) const
{
    // Only called for attached lists
    REALM_ASSERT(is_attached());

    REALM_ASSERT_3(m_row_indexes.get_root_array()->get_ndx_in_parent(), ==, row_ndx);
    bool not_degenerate = m_row_indexes.get_root_array()->get_ref_from_parent() != 0;
    REALM_ASSERT_3(not_degenerate, ==, m_row_indexes.is_attached());
    if (m_row_indexes.is_attached())
        m_row_indexes.verify();
}

#endif // REALM_DEBUG
