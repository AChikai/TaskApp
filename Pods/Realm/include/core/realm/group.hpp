/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/

#ifndef REALM_GROUP_HPP
#define REALM_GROUP_HPP

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

#include <realm/util/features.h>
#include <realm/exceptions.hpp>
#include <realm/impl/input_stream.hpp>
#include <realm/impl/output_stream.hpp>
#include <realm/table.hpp>
#include <realm/table_basic_fwd.hpp>
#include <realm/alloc_slab.hpp>

namespace realm {

class SharedGroup;
namespace _impl {
class GroupFriend;
class TransactLogConvenientEncoder;
class TransactLogParser;
}


/// A group is a collection of named tables.
///
/// Tables occur in the group in an unspecified order, but an order that
/// generally remains fixed. The order is guaranteed to remain fixed between two
/// points in time if no tables are added to, or removed from the group during
/// that time. When tables are added to, or removed from the group, the order
/// may change arbitrarily.
///
/// If `table` is a table accessor attached to a group-level table, and `group`
/// is a group accessor attached to the group, then the following is guaranteed,
/// even after a change in the table order:
///
/// \code{.cpp}
///
///     table == group.get_table(table.get_index_in_group())
///
/// \endcode
///
class Group: private Table::Parent {
public:
    /// Construct a free-standing group. This group instance will be
    /// in the attached state, but neither associated with a file, nor
    /// with an external memory buffer.
    Group();

    enum OpenMode {
        /// Open in read-only mode. Fail if the file does not already exist.
        mode_ReadOnly,
        /// Open in read/write mode. Create the file if it doesn't exist.
        mode_ReadWrite,
        /// Open in read/write mode. Fail if the file does not already exist.
        mode_ReadWriteNoCreate
    };

    /// Equivalent to calling open(const std::string&, const char*, OpenMode)
    /// on an unattached group accessor.
    explicit Group(const std::string& file, const char* encryption_key = nullptr, OpenMode = mode_ReadOnly);

    /// Equivalent to calling open(BinaryData, bool) on an unattached
    /// group accessor. Note that if this constructor throws, the
    /// ownership of the memory buffer will remain with the caller,
    /// regardless of whether \a take_ownership is set to `true` or
    /// `false`.
    explicit Group(BinaryData, bool take_ownership = true);

    struct unattached_tag {};

    /// Create a Group instance in its unattached state. It may then
    /// be attached to a database file later by calling one of the
    /// open() methods. You may test whether this instance is
    /// currently in its attached state by calling
    /// is_attached(). Calling any other method (except the
    /// destructor) while in the unattached state has undefined
    /// behavior.
    Group(unattached_tag) noexcept;

    // FIXME: Implement a proper copy constructor (fairly trivial).
    Group(const Group&) = delete;

    ~Group() noexcept override;

    /// Attach this Group instance to the specified database file.
    ///
    /// By default, the specified file is opened in read-only mode
    /// (mode_ReadOnly). This allows opening a file even when the
    /// caller lacks permission to write to that file. The opened
    /// group may still be modified freely, but the changes cannot be
    /// written back to the same file using the commit() function. An
    /// attempt to do that, will cause an exception to be thrown. When
    /// opening in read-only mode, it is an error if the specified
    /// file does not already exist in the file system.
    ///
    /// Alternatively, the file can be opened in read/write mode
    /// (mode_ReadWrite). This allows use of the commit() function,
    /// but, of course, it also requires that the caller has
    /// permission to write to the specified file. When opening in
    /// read-write mode, an attempt to create the specified file will
    /// be made, if it does not already exist in the file system.
    ///
    /// In any case, if the file already exists, it must contain a
    /// valid Realm database. In many cases invalidity will be
    /// detected and cause the InvalidDatabase exception to be thrown,
    /// but you should not rely on it.
    ///
    /// Note that changes made to the database via a Group instance
    /// are not automatically committed to the specified file. You
    /// may, however, at any time, explicitly commit your changes by
    /// calling the commit() method, provided that the specified
    /// open-mode is not mode_ReadOnly. Alternatively, you may call
    /// write() to write the entire database to a new file. Writing
    /// the database to a new file does not end, or in any other way
    /// change the association between the Group instance and the file
    /// that was specified in the call to open().
    ///
    /// A file that is passed to Group::open(), may not be modified by
    /// a third party until after the Group object is
    /// destroyed. Behavior is undefined if a file is modified by a
    /// third party while any Group object is associated with it.
    ///
    /// Calling open() on a Group instance that is already in the
    /// attached state has undefined behavior.
    ///
    /// Accessing a Realm database file through manual construction
    /// of a Group object does not offer any level of thread safety or
    /// transaction safety. When any of those kinds of safety are a
    /// concern, consider using a SharedGroup instead. When accessing
    /// a database file in read/write mode through a manually
    /// constructed Group object, it is entirely the responsibility of
    /// the application that the file is not accessed in any way by a
    /// third party during the life-time of that group object. It is,
    /// on the other hand, safe to concurrently access a database file
    /// by multiple manually created Group objects, as long as all of
    /// them are opened in read-only mode, and there is no other party
    /// that modifies the file concurrently.
    ///
    /// Do not call this function on a group instance that is managed
    /// by a shared group. Doing so will result in undefined behavior.
    ///
    /// Even if this function throws, it may have the side-effect of
    /// creating the specified file, and the file may get left behind
    /// in an invalid state. Of course, this can only happen if
    /// read/write mode (mode_ReadWrite) was requested, and the file
    /// did not already exist.
    ///
    /// \param file File system path to a Realm database file.
    ///
    /// \param encryption_key 32-byte key used to encrypt and decrypt
    /// the database file, or nullptr to disable encryption.
    ///
    /// \param mode Specifying a mode that is not mode_ReadOnly
    /// requires that the specified file can be opened in read/write
    /// mode. In general there is no reason to open a group in
    /// read/write mode unless you want to be able to call
    /// Group::commit().
    ///
    /// \throw util::File::AccessError If the file could not be
    /// opened. If the reason corresponds to one of the exception
    /// types that are derived from util::File::AccessError, the
    /// derived exception type is thrown. Note that InvalidDatabase is
    /// among these derived exception types.
    void open(const std::string& file, const char* encryption_key = nullptr,
              OpenMode mode = mode_ReadOnly);

    /// Attach this Group instance to the specified memory buffer.
    ///
    /// This is similar to constructing a group from a file except
    /// that in this case the database is assumed to be stored in the
    /// specified memory buffer.
    ///
    /// If \a take_ownership is `true`, you pass the ownership of the
    /// specified buffer to the group. In this case the buffer will
    /// eventually be freed using std::free(), so the buffer you pass,
    /// must have been allocated using std::malloc().
    ///
    /// On the other hand, if \a take_ownership is set to `false`, it
    /// is your responsibility to keep the memory buffer alive during
    /// the lifetime of the group, and in case the buffer needs to be
    /// deallocated afterwards, that is your responsibility too.
    ///
    /// If this function throws, the ownership of the memory buffer
    /// will remain with the caller, regardless of whether \a
    /// take_ownership is set to `true` or `false`.
    ///
    /// Calling open() on a Group instance that is already in the
    /// attached state has undefined behavior.
    ///
    /// Do not call this function on a group instance that is managed
    /// by a shared group. Doing so will result in undefined behavior.
    ///
    /// \throw InvalidDatabase If the specified buffer does not appear
    /// to contain a valid database.
    void open(BinaryData, bool take_ownership = true);

    /// A group may be created in the unattached state, and then later
    /// attached to a file with a call to open(). Calling any method
    /// other than open(), and is_attached() on an unattached instance
    /// results in undefined behavior.
    bool is_attached() const noexcept;

    /// Returns true if, and only if the number of tables in this
    /// group is zero.
    bool is_empty() const noexcept;

    /// Returns the number of tables in this group.
    size_t size() const;

    //@{

    /// has_table() returns true if, and only if this group contains a table
    /// with the specified name.
    ///
    /// find_table() returns the index of the first table in this group with the
    /// specified name, or `realm::not_found` if this group does not contain a
    /// table with the specified name.
    ///
    /// get_table_name() returns the name of table at the specified index.
    ///
    /// The versions of get_table(), that accepts a \a name argument, return the
    /// first table with the specified name, or null if no such table exists.
    ///
    /// add_table() adds a table with the specified name to this group. It
    /// throws TableNameInUse if \a require_unique_name is true and \a name
    /// clashes with the name of an existing table. If \a require_unique_name is
    /// false, it is possible to add more than one table with the same
    /// name. Whenever a table is added, the order of the preexisting tables may
    /// change arbitrarily, and the new table may not end up as the last one
    /// either. But know that you can always call Table::get_index_in_group() on
    /// the returned table accessor to find out at which index it ends up.
    ///
    /// get_or_add_table() checks if a table exists in this group with the specified
    /// name. If it doesn't exist, a table is created.
    ///
    /// get_or_insert_table() works slightly differently from get_or_add_table(),
    /// in that it considers the position of the requested table as part of that
    /// table's identifying "key", in addition to the name.
    ///
    /// remove_table() removes the specified table from this group. A table can
    /// be removed only when it is not the target of a link column of a
    /// different table. Whenever a table is removed, the order of the remaining
    /// tables may change arbitrarily.
    ///
    /// rename_table() changes the name of a preexisting table. If \a
    /// require_unique_name is false, it becomes possible to have more than one
    /// table with a given name in a single group.
    ///
    /// The template functions work exactly like their non-template namesakes
    /// except as follows: The template versions of get_table() and
    /// get_or_add_table() throw DescriptorMismatch if the dynamic type of the
    /// specified table does not match the statically specified custom table
    /// type. The template versions of add_table() and get_or_add_table() set
    /// the dynamic type (descriptor) to match the statically specified custom
    /// table type.
    ///
    /// \tparam T An instance of the BasicTable class template.
    ///
    /// \param index Index of table in this group.
    ///
    /// \param name Name of table. All strings are valid table names as long as
    /// they are valid UTF-8 encodings and the number of bytes does not exceed
    /// `max_table_name_length`. A call to add_table() or get_or_add_table()
    /// with a name that is longer than `max_table_name_length` will cause an
    /// exception to be thrown.
    ///
    /// \param new_name New name for preexisting table.
    ///
    /// \param require_unique_name When set to true (the default), it becomes
    /// impossible to add a table with a name that is already in use, or to
    /// rename a table to a name that is already in use.
    ///
    /// \param was_added When specified, the boolean variable is set to true if
    /// the table was added, and to false otherwise. If the function throws, the
    /// boolean variable retains its original value.
    ///
    /// \return get_table(), add_table(), and get_or_add_table() return a table
    /// accessor attached to the requested (or added) table. get_table() may
    /// return null.
    ///
    /// \throw DescriptorMismatch Thrown by get_table() and get_or_add_table()
    /// tf the dynamic table type does not match the statically specified custom
    /// table type (\a T).
    ///
    /// \throw NoSuchTable Thrown by remove_table() and rename_table() if there
    /// is no table with the specified \a name.
    ///
    /// \throw TableNameInUse Thrown by add_table() if \a require_unique_name is
    /// true and \a name clashes with the name of a preexisting table. Thrown by
    /// rename_table() if \a require_unique_name is true and \a new_name clashes
    /// with the name of a preexisting table.
    ///
    /// \throw CrossTableLinkTarget Thrown by remove_table() if the specified
    /// table is the target of a link column of a different table.

    static const size_t max_table_name_length = 63;

    bool has_table(StringData name) const noexcept;
    size_t find_table(StringData name) const noexcept;
    StringData get_table_name(size_t table_ndx) const;

    TableRef get_table(size_t index);
    ConstTableRef get_table(size_t index) const;

    TableRef get_table(StringData name);
    ConstTableRef get_table(StringData name) const;

    TableRef add_table(StringData name, bool require_unique_name = true);
    TableRef insert_table(size_t index, StringData name, bool require_unique_name = true);
    TableRef get_or_add_table(StringData name, bool* was_added = nullptr);
    TableRef get_or_insert_table(size_t index, StringData name, bool* was_added = nullptr);

    template<class T>
    BasicTableRef<T> get_table(size_t index);

    template<class T>
    BasicTableRef<const T> get_table(size_t index) const;

    template<class T>
    BasicTableRef<T> get_table(StringData name);

    template<class T>
    BasicTableRef<const T> get_table(StringData name) const;

    template<class T>
    BasicTableRef<T> add_table(StringData name, bool require_unique_name = true);

    template<class T>
    BasicTableRef<T> insert_table(size_t index, StringData name, bool require_unique_name = true);

    template<class T>
    BasicTableRef<T> get_or_add_table(StringData name, bool* was_added = nullptr);

    template<class T>
    BasicTableRef<T> get_or_insert_table(size_t index, StringData name, bool* was_added = nullptr);

    void remove_table(size_t index);
    void remove_table(StringData name);

    void rename_table(size_t index, StringData new_name, bool require_unique_name = true);
    void rename_table(StringData name, StringData new_name, bool require_unique_name = true);

    //@}

    void move_table(size_t index_1, size_t index_2);

    // Serialization

    /// Write this database to the specified output stream.
    ///
    /// \param pad If true, the file is padded to ensure the footer is aligned
    /// to the end of a page
    void write(std::ostream&, bool pad=false) const;

    /// Write this database to a new file. It is an error to specify a
    /// file that already exists. This is to protect against
    /// overwriting a database file that is currently open, which
    /// would cause undefined behaviour.
    ///
    /// \param file A filesystem path.
    ///
    /// \param encryption_key 32-byte key used to encrypt the database file,
    /// or nullptr to disable encryption.
    ///
    /// \throw util::File::AccessError If the file could not be
    /// opened. If the reason corresponds to one of the exception
    /// types that are derived from util::File::AccessError, the
    /// derived exception type is thrown. In particular,
    /// util::File::Exists will be thrown if the file exists already.
    void write(const std::string& file, const char* encryption_key=0) const;

    /// Write this database to a memory buffer.
    ///
    /// Ownership of the returned buffer is transferred to the
    /// caller. The memory will have been allocated using
    /// std::malloc().
    BinaryData write_to_mem() const;

    /// Commit changes to the attached file. This requires that the
    /// attached file is opened in read/write mode.
    ///
    /// Calling this function on an unattached group, a free-standing
    /// group, a group whose attached file is opened in read-only
    /// mode, a group that is attached to a memory buffer, or a group
    /// that is managed by a shared group, is an error and will result
    /// in undefined behavior.
    ///
    /// Table accesors will remain valid across the commit. Note that
    /// this is not the case when working with proper transactions.
    void commit();

    //@{
    /// Some operations on Tables in a Group can cause indirect changes to other
    /// fields, including in other Tables in the same Group. Specifically,
    /// removing a row will set any links to that row to null, and if it had the
    /// last strong links to other rows, will remove those rows. When this
    /// happens, The cascade notification handler will be called with a
    /// CascadeNotification containing information about what indirect changes
    /// will occur, before any changes are made.
    ///
    /// has_cascade_notification_handler() returns true if and only if there is
    /// currently a non-null notification handler registered.
    ///
    /// set_cascade_notification_handler() replaces the current handler (if any)
    /// with the passed in handler. Pass in nullptr to remove the current handler
    /// without registering a new one.
    ///
    /// CascadeNotification contains a vector of rows which will be removed and
    /// a vector of links which will be set to null (or removed, for entries in
    /// LinkLists).
    struct CascadeNotification {
        struct row {
            /// Non-zero iff the removal of this row is ordered
            /// (Table::remove()), as opposed to ordered
            /// (Table::move_last_over()). Implicit removals are always
            /// unordered.
            ///
            /// This flag does not take part in comparisons (operator==() and
            /// operator<()).
            size_t is_ordered_removal : 1;

            /// Index within group of a group-level table.
            size_t table_ndx : std::numeric_limits<size_t>::digits - 1;

            /// Row index which will be removed.
            size_t row_ndx;

            row(): is_ordered_removal(0) {}

            bool operator==(const row&) const noexcept;
            bool operator!=(const row&) const noexcept;

            /// Trivial lexicographic order
            bool operator<(const row&) const noexcept;
        };

        struct link {
            const Table* origin_table; ///< A group-level table.
            size_t origin_col_ndx; ///< Link column being nullified.
            size_t origin_row_ndx; ///< Row in column being nullified.
            /// The target row index which is being removed. Mostly relevant for
            /// LinkList (to know which entries are being removed), but also
            /// valid for Link.
            size_t old_target_row_ndx;
        };

        /// A sorted list of rows which will be removed by the current operation.
        std::vector<row> rows;

        /// An unordered list of links which will be nullified by the current
        /// operation.
        std::vector<link> links;
    };

    bool has_cascade_notification_handler() const noexcept;
    void set_cascade_notification_handler(std::function<void (const CascadeNotification&)> new_handler) noexcept;

    //@}

    //@{
    /// During sync operation, schema changes may happen at runtime as connected
    /// clients update their schema as part of an app update. Since this is a
    /// relatively rare event, no attempt is made at limiting the amount of work
    /// the handler is required to do to update its information about table and
    /// column indices (i.e., all table and column indices must be recalculated).
    ///
    /// At the time of writing, only additive schema changes may occur in that
    /// scenario.
    ///
    /// has_schema_change_notification_handler() returns true iff there is currently
    /// a non-null notification handler registered.
    ///
    /// set_schema_change_notification_handler() replaces the current handler (if any)
    /// with the passed in handler. Pass in nullptr to remove the current handler
    /// without registering a new one.

    bool has_schema_change_notification_handler() const noexcept;
    void set_schema_change_notification_handler(std::function<void()> new_handler) noexcept;

    //@}

    // Conversion
    template<class S>
    void to_json(S& out, size_t link_depth = 0,
        std::map<std::string, std::string>* renames = nullptr) const;
    void to_string(std::ostream& out) const;

    /// Compare two groups for equality. Two groups are equal if, and
    /// only if, they contain the same tables in the same order, that
    /// is, for each table T at index I in one of the groups, there is
    /// a table at index I in the other group that is equal to T.
    bool operator==(const Group&) const;

    /// Compare two groups for inequality. See operator==().
    bool operator!=(const Group& g) const { return !(*this == g); }

#ifdef REALM_DEBUG
    void verify() const;
    void print() const;
    void print_free() const;
    MemStats stats();
    void enable_mem_diagnostics(bool enable = true) { m_alloc.enable_debug(enable); }
    void to_dot(std::ostream&) const;
    void to_dot() const; // To std::cerr (for GDB)
    void to_dot(const char* file_path) const;
#else
    void verify() const {}
#endif

private:
    SlabAlloc m_alloc;

    /// `m_top` is the root node of the Realm, and has the following layout:
    ///
    /// <pre>
    ///
    ///   slot  value
    ///   -----------------------
    ///   1st   m_table_names
    ///   2nd   m_tables
    ///   3rd   Logical file size
    ///   4th   GroupWriter::m_free_positions (optional)
    ///   5th   GroupWriter::m_free_lengths   (optional)
    ///   6th   GroupWriter::m_free_versions  (optional)
    ///   7th   Transaction number / version  (optional)
    ///   8th   Synchronization history       (optional)
    ///
    ///
    /// </pre>
    ///
    /// The first three entries are mandatory. In files created by
    /// Group::write(), none of the optional entries are present. In files
    /// updated by Group::commit(), the 4th and 5th entry is present. In files
    /// updated by way of a transaction (SharedGroup::commit()), the 4th, 5th,
    /// 6th, and 7th entry is present.
    Array m_top;
    ArrayInteger m_tables;
    ArrayString m_table_names;

    static constexpr int s_sync_history_ndx_in_parent = 7;

    typedef std::vector<Table*> table_accessors;
    mutable table_accessors m_table_accessors;

    const bool m_is_shared;

    std::function<void (const CascadeNotification&)> m_notify_handler;
    std::function<void ()> m_schema_change_handler;

    struct shared_tag {};
    Group(shared_tag) noexcept;

    void init_array_parents() noexcept;

    /// If `top_ref` is not zero, attach this group accessor to the specified
    /// underlying node structure. If `top_ref` is zero, create a new node
    /// structure that represents an empty group, and attach this group accessor
    /// to it. It is an error to call this function on an already attached group
    /// accessor.
    void attach(ref_type top_ref);

    /// Detach this group accessor from the underlying node structure. If this
    /// group accessors is already in the detached state, this function does
    /// nothing (idempotency).
    void detach() noexcept;

    void attach_shared(ref_type new_top_ref, size_t new_file_size);

    void reset_free_space_tracking();

    void remap_and_update_refs(ref_type new_top_ref, size_t new_file_size);

    /// Recursively update refs stored in all cached array
    /// accessors. This includes cached array accessors in any
    /// currently attached table accessors. This ensures that the
    /// group instance itself, as well as any attached table accessor
    /// that exists across Group::commit() will remain valid. This
    /// function is not appropriate for use in conjunction with
    /// commits via shared group.
    void update_refs(ref_type top_ref, size_t old_baseline) noexcept;

    // Overriding method in ArrayParent
    void update_child_ref(size_t, ref_type) override;

    // Overriding method in ArrayParent
    ref_type get_child_ref(size_t) const noexcept override;

    // Overriding method in Table::Parent
    StringData get_child_name(size_t) const noexcept override;

    // Overriding method in Table::Parent
    void child_accessor_destroyed(Table*) noexcept override;

    // Overriding method in Table::Parent
    Group* get_parent_group() noexcept override;

    class TableWriter;
    class DefaultTableWriter;

    static void write(std::ostream&, TableWriter&, bool, uint_fast64_t = 0);

    typedef void (*DescSetter)(Table&);
    typedef bool (*DescMatcher)(const Spec&);

    Table* do_get_table(size_t table_ndx, DescMatcher desc_matcher);
    const Table* do_get_table(size_t table_ndx, DescMatcher desc_matcher) const;
    Table* do_get_table(StringData name, DescMatcher desc_matcher);
    const Table* do_get_table(StringData name, DescMatcher desc_matcher) const;
    Table* do_insert_table(size_t, StringData name, DescSetter desc_setter, bool require_unique_name);
    Table* do_insert_table(size_t, StringData name, DescSetter desc_setter);
    Table* do_get_or_add_table(StringData name, DescMatcher desc_matcher, DescSetter setter,
                               bool* was_added);
    Table* do_get_or_insert_table(size_t, StringData name, DescMatcher desc_matcher,
                                  DescSetter desc_setter, bool* was_added);

    void create_and_insert_table(size_t new_table_ndx, StringData name);
    Table* create_table_accessor(size_t table_ndx);

    void detach_table_accessors() noexcept; // Idempotent

    void mark_all_table_accessors() noexcept;

    void write(const std::string& file, const char* encryption_key,
               uint_fast64_t version_number) const;
    void write(std::ostream&, bool pad, uint_fast64_t version_numer) const;

    Replication* get_replication() const noexcept;
    void set_replication(Replication*) noexcept;
    class TransactAdvancer;
    void advance_transact(ref_type new_top_ref, size_t new_file_size,
                          _impl::NoCopyInputStream&);
    void refresh_dirty_accessors();
    template<class F>
    void update_table_indices(F&& map_function);

    int get_file_format() const noexcept;
    void set_file_format(int) noexcept;
    int get_committed_file_format() const noexcept;

    /// Must be called from within a write transaction
    void upgrade_file_format();

#ifdef REALM_DEBUG
    std::pair<ref_type, size_t>
    get_to_dot_parent(size_t ndx_in_parent) const override;
#endif

    void send_cascade_notification(const CascadeNotification& notification) const;
    void send_schema_change_notification() const;

    void set_sync_history_parent(Array& sync_history_root);

    friend class Table;
    friend class GroupWriter;
    friend class SharedGroup;
    friend class _impl::GroupFriend;
    friend class _impl::TransactLogConvenientEncoder;
    friend class _impl::TransactLogParser;
    friend class Replication;
    friend class TrivialReplication;
};





// Implementation

inline Group::Group():
    m_alloc(), // Throws
    m_top(m_alloc),
    m_tables(m_alloc),
    m_table_names(m_alloc),
    m_is_shared(false)
{
    init_array_parents();
    m_alloc.attach_empty(); // Throws
    ref_type top_ref = 0; // Instantiate a new empty group
    attach(top_ref); // Throws
}

inline Group::Group(const std::string& file, const char* key, OpenMode mode):
    m_alloc(), // Throws
    m_top(m_alloc),
    m_tables(m_alloc),
    m_table_names(m_alloc),
    m_is_shared(false)
{
    init_array_parents();

    open(file, key, mode); // Throws
}

inline Group::Group(BinaryData buffer, bool take_ownership):
    m_alloc(), // Throws
    m_top(m_alloc),
    m_tables(m_alloc),
    m_table_names(m_alloc),
    m_is_shared(false)
{
    init_array_parents();
    open(buffer, take_ownership); // Throws
}

inline Group::Group(unattached_tag) noexcept:
    m_alloc(), // Throws
    m_top(m_alloc),
    m_tables(m_alloc),
    m_table_names(m_alloc),
    m_is_shared(false)
{
    init_array_parents();
}

inline Group* Group::get_parent_group() noexcept
{
    return this;
}

inline Group::Group(shared_tag) noexcept:
    m_alloc(), // Throws
    m_top(m_alloc),
    m_tables(m_alloc),
    m_table_names(m_alloc),
    m_is_shared(true)
{
    init_array_parents();
}

inline bool Group::is_attached() const noexcept
{
    return m_top.is_attached();
}

inline bool Group::is_empty() const noexcept
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    REALM_ASSERT(m_table_names.is_attached());
    return m_table_names.is_empty();
}

inline size_t Group::size() const
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    REALM_ASSERT(m_table_names.is_attached());
    return m_table_names.size();
}

inline StringData Group::get_table_name(size_t table_ndx) const
{
    if (table_ndx >= size())
        throw LogicError(LogicError::table_index_out_of_range);
    return m_table_names.get(table_ndx);
}

inline bool Group::has_table(StringData name) const noexcept
{
    size_t ndx = find_table(name);
    return ndx != not_found;
}

inline size_t Group::find_table(StringData name) const noexcept
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    REALM_ASSERT(m_table_names.is_attached());
    size_t ndx = m_table_names.find_first(name);
    return ndx;
}

inline TableRef Group::get_table(size_t table_ndx)
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = nullptr; // Do not check descriptor
    Table* table = do_get_table(table_ndx, desc_matcher); // Throws
    return TableRef(table);
}

inline ConstTableRef Group::get_table(size_t table_ndx) const
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = nullptr; // Do not check descriptor
    const Table* table = do_get_table(table_ndx, desc_matcher); // Throws
    return ConstTableRef(table);
}

inline TableRef Group::get_table(StringData name)
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = nullptr; // Do not check descriptor
    Table* table = do_get_table(name, desc_matcher); // Throws
    return TableRef(table);
}

inline ConstTableRef Group::get_table(StringData name) const
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = nullptr; // Do not check descriptor
    const Table* table = do_get_table(name, desc_matcher); // Throws
    return ConstTableRef(table);
}

inline TableRef Group::insert_table(size_t table_ndx, StringData name, bool require_unique_name)
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescSetter desc_setter = nullptr; // Do not add any columns
    Table* table = do_insert_table(table_ndx, name, desc_setter, require_unique_name); // Throws
    return TableRef(table);
}

inline TableRef Group::add_table(StringData name, bool require_unique_name)
{
    return insert_table(size(), name, require_unique_name);
}

inline TableRef Group::get_or_insert_table(size_t table_ndx, StringData name, bool* was_added)
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = nullptr; // Do not check descriptor
    DescSetter desc_setter = nullptr; // Do not add any columns
    Table* table = do_get_or_insert_table(table_ndx, name, desc_matcher, desc_setter, was_added); // Throws
    return TableRef(table);
}

inline TableRef Group::get_or_add_table(StringData name, bool* was_added)
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = nullptr; // Do not check descriptor
    DescSetter desc_setter = nullptr; // Do not add any columns
    Table* table = do_get_or_add_table(name, desc_matcher, desc_setter, was_added); // Throws
    return TableRef(table);
}

template<class T>
inline BasicTableRef<T> Group::get_table(size_t table_ndx)
{
    static_assert(IsBasicTable<T>::value, "Invalid table type");
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = &T::matches_dynamic_type;
    Table* table = do_get_table(table_ndx, desc_matcher); // Throws
    return BasicTableRef<T>(static_cast<T*>(table));
}

template<class T>
inline BasicTableRef<const T> Group::get_table(size_t table_ndx) const
{
    static_assert(IsBasicTable<T>::value, "Invalid table type");
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = &T::matches_dynamic_type;
    const Table* table = do_get_table(table_ndx, desc_matcher); // Throws
    return BasicTableRef<const T>(static_cast<const T*>(table));
}

template<class T>
inline BasicTableRef<T> Group::get_table(StringData name)
{
    static_assert(IsBasicTable<T>::value, "Invalid table type");
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = &T::matches_dynamic_type;
    Table* table = do_get_table(name, desc_matcher); // Throws
    return BasicTableRef<T>(static_cast<T*>(table));
}

template<class T>
inline BasicTableRef<const T> Group::get_table(StringData name) const
{
    static_assert(IsBasicTable<T>::value, "Invalid table type");
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = &T::matches_dynamic_type;
    const Table* table = do_get_table(name, desc_matcher); // Throws
    return BasicTableRef<const T>(static_cast<const T*>(table));
}

template<class T>
inline BasicTableRef<T> Group::insert_table(size_t table_ndx, StringData name,
                                            bool require_unique_name)
{
    static_assert(IsBasicTable<T>::value, "Invalid table type");
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescSetter desc_setter = &T::set_dynamic_type;
    Table* table = do_insert_table(table_ndx, name, desc_setter, require_unique_name); // Throws
    return BasicTableRef<T>(static_cast<T*>(table));
}

template<class T>
inline BasicTableRef<T> Group::add_table(StringData name, bool require_unique_name)
{
    return insert_table<T>(size(), name, require_unique_name);
}

template<class T>
BasicTableRef<T> Group::get_or_insert_table(size_t table_ndx, StringData name, bool* was_added)
{
    static_assert(IsBasicTable<T>::value, "Invalid table type");
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = &T::matches_dynamic_type;
    DescSetter desc_setter = &T::set_dynamic_type;
    Table* table = do_get_or_insert_table(table_ndx, name, desc_matcher,
                                          desc_setter, was_added); // Throws
    return BasicTableRef<T>(static_cast<T*>(table));
}

template<class T>
BasicTableRef<T> Group::get_or_add_table(StringData name, bool* was_added)
{
    static_assert(IsBasicTable<T>::value, "Invalid table type");
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);
    DescMatcher desc_matcher = &T::matches_dynamic_type;
    DescSetter desc_setter = &T::set_dynamic_type;
    Table* table = do_get_or_add_table(name, desc_matcher,
                                       desc_setter, was_added); // Throws
    return BasicTableRef<T>(static_cast<T*>(table));
}

template<class S>
void Group::to_json(S& out, size_t link_depth,
                    std::map<std::string, std::string>* renames) const
{
    if (!is_attached())
        throw LogicError(LogicError::detached_accessor);

    std::map<std::string, std::string> renames2;
    renames = renames ? renames : &renames2;

    out << "{";

    for (size_t i = 0; i < m_tables.size(); ++i) {
        StringData name = m_table_names.get(i);
        std::map<std::string, std::string>& m = *renames;
        if (m[name] != "")
            name = m[name];

        ConstTableRef table = get_table(i);

        if (i)
            out << ",";
        out << "\"" << name << "\"";
        out << ":";
        table->to_json(out, link_depth, renames);
    }

    out << "}";
}

inline void Group::init_array_parents() noexcept
{
    m_table_names.set_parent(&m_top, 0);
    m_tables.set_parent(&m_top, 1);
}

inline void Group::update_child_ref(size_t child_ndx, ref_type new_ref)
{
    m_tables.set(child_ndx, new_ref);
}

inline ref_type Group::get_child_ref(size_t child_ndx) const noexcept
{
    return m_tables.get_as_ref(child_ndx);
}

inline StringData Group::get_child_name(size_t child_ndx) const noexcept
{
    return m_table_names.get(child_ndx);
}

inline void Group::child_accessor_destroyed(Table*) noexcept
{
    // Ignore
}

inline bool Group::has_cascade_notification_handler() const noexcept
{
    return !!m_notify_handler;
}

inline void Group::set_cascade_notification_handler(std::function<void (const CascadeNotification&)> new_handler) noexcept
{
    m_notify_handler = std::move(new_handler);
}

inline void Group::send_cascade_notification(const CascadeNotification& notification) const
{
    if (m_notify_handler)
        m_notify_handler(notification);
}

inline bool Group::has_schema_change_notification_handler() const noexcept
{
    return !!m_schema_change_handler;
}

inline void Group::set_schema_change_notification_handler(std::function<void ()> new_handler) noexcept
{
    m_schema_change_handler = std::move(new_handler);
}

inline void Group::send_schema_change_notification() const
{
    if (m_schema_change_handler)
        m_schema_change_handler();
}

inline void Group::set_sync_history_parent(Array& sync_history_root)
{
    REALM_ASSERT(m_top.is_attached());
    REALM_ASSERT(m_top.size() >= 3);
    while (m_top.size() <= s_sync_history_ndx_in_parent)
        m_top.add(0); // Throws
    sync_history_root.set_parent(&m_top, s_sync_history_ndx_in_parent);
}

class Group::TableWriter {
public:
    virtual ref_type write_names(_impl::OutputStream&) = 0;
    virtual ref_type write_tables(_impl::OutputStream&) = 0;
    virtual ~TableWriter() noexcept {}
};

inline const Table* Group::do_get_table(size_t table_ndx, DescMatcher desc_matcher) const
{
    return const_cast<Group*>(this)->do_get_table(table_ndx, desc_matcher); // Throws
}

inline const Table* Group::do_get_table(StringData name, DescMatcher desc_matcher) const
{
    return const_cast<Group*>(this)->do_get_table(name, desc_matcher); // Throws
}

inline void Group::reset_free_space_tracking()
{
    m_alloc.reset_free_space_tracking(); // Throws
}

inline Replication* Group::get_replication() const noexcept
{
    return m_alloc.get_replication();
}

inline void Group::set_replication(Replication* repl) noexcept
{
    m_alloc.set_replication(repl);
}

// The purpose of this class is to give internal access to some, but
// not all of the non-public parts of the Group class.
class _impl::GroupFriend {
public:
    static Allocator& get_alloc(Group& group) noexcept
    {
        return group.m_alloc;
    }

    static Table& get_table(Group& group, size_t ndx_in_group)
    {
        Group::DescMatcher desc_matcher = 0; // Do not check descriptor
        Table* table = group.do_get_table(ndx_in_group, desc_matcher); // Throws
        return *table;
    }

    static const Table& get_table(const Group& group, size_t ndx_in_group)
    {
        Group::DescMatcher desc_matcher = 0; // Do not check descriptor
        const Table* table = group.do_get_table(ndx_in_group, desc_matcher); // Throws
        return *table;
    }

    static Table* get_table(Group& group, StringData name)
    {
        Group::DescMatcher desc_matcher = 0; // Do not check descriptor
        Table* table = group.do_get_table(name, desc_matcher); // Throws
        return table;
    }

    static const Table* get_table(const Group& group, StringData name)
    {
        Group::DescMatcher desc_matcher = 0; // Do not check descriptor
        const Table* table = group.do_get_table(name, desc_matcher); // Throws
        return table;
    }

    static Table& insert_table(Group& group, size_t table_ndx, StringData name,
                               bool require_unique_name)
    {
        Group::DescSetter desc_setter = nullptr; // Do not add any columns
        return *group.do_insert_table(table_ndx, name, desc_setter, require_unique_name);
    }

    static Table& add_table(Group& group, StringData name, bool require_unique_name)
    {
        return insert_table(group, group.size(), name, require_unique_name);
    }

    static Table& get_or_insert_table(Group& group, size_t table_ndx, StringData name,
                                      bool* was_inserted)
    {
        Group::DescMatcher desc_matcher = nullptr; // Do not check descriptor
        Group::DescSetter  desc_setter  = nullptr; // Do not add any columns
        return *group.do_get_or_insert_table(table_ndx, name, desc_matcher, desc_setter, was_inserted);
    }

    static Table& get_or_add_table(Group& group, StringData name, bool* was_inserted)
    {
        Group::DescMatcher desc_matcher = nullptr; // Do not check descriptor
        Group::DescSetter  desc_setter  = nullptr; // Do not add any columns
        return *group.do_get_or_add_table(name, desc_matcher, desc_setter, was_inserted);
    }

    static void send_cascade_notification(const Group& group, const Group::CascadeNotification& notification)
    {
        group.send_cascade_notification(notification);
    }

    static Replication* get_replication(const Group& group) noexcept
    {
        return group.get_replication();
    }

    static void set_replication(Group& group, Replication* repl) noexcept
    {
        group.set_replication(repl);
    }

    static void detach(Group& group) noexcept
    {
        group.detach();
    }

    static void attach_shared(Group& group, ref_type new_top_ref, size_t new_file_size)
    {
        group.attach_shared(new_top_ref, new_file_size); // Throws
    }

    static void reset_free_space_tracking(Group& group)
    {
        group.reset_free_space_tracking(); // Throws
    }

    static void remap_and_update_refs(Group& group, ref_type new_top_ref, size_t new_file_size)
    {
        group.remap_and_update_refs(new_top_ref, new_file_size); // Throws
    }

    static void advance_transact(Group& group, ref_type new_top_ref, size_t new_file_size,
                                 _impl::NoCopyInputStream& in)
    {
        group.advance_transact(new_top_ref, new_file_size, in); // Throws
    }

    static void set_sync_history_parent(Group& group, Array& sync_history_root)
    {
        group.set_sync_history_parent(sync_history_root); // Throws
    }
};


struct CascadeState: Group::CascadeNotification {
    /// If non-null, then no recursion will be performed for rows of that
    /// table. The effect is then exactly as if all the rows of that table were
    /// added to \a state.rows initially, and then removed again after the
    /// explicit invocations of Table::cascade_break_backlinks_to() (one for
    /// each initiating row). This is used by Table::clear() to avoid
    /// reentrance.
    ///
    /// Must never be set concurrently with stop_on_link_list_column.
    Table* stop_on_table = nullptr;

    /// If non-null, then Table::cascade_break_backlinks_to() will skip the
    /// removal of reciprocal backlinks for the link list at
    /// stop_on_link_list_row_ndx in this column, and no recursion will happen
    /// on its behalf. This is used by LinkView::clear() to avoid reentrance.
    ///
    /// Must never be set concurrently with stop_on_table.
    LinkListColumn* stop_on_link_list_column = nullptr;

    /// Is ignored if stop_on_link_list_column is null.
    size_t stop_on_link_list_row_ndx = 0;

    /// If false, the links field is not needed, so any work done just for that
    /// can be skipped.
    bool track_link_nullifications = false;
};

inline bool Group::CascadeNotification::row::operator==(const row& r) const noexcept
{
    return table_ndx == r.table_ndx && row_ndx == r.row_ndx;
}

inline bool Group::CascadeNotification::row::operator!=(const row& r) const noexcept
{
    return !(*this == r);
}

inline bool Group::CascadeNotification::row::operator<(const row& r) const noexcept
{
    return table_ndx < r.table_ndx || (table_ndx == r.table_ndx && row_ndx < r.row_ndx);
}

} // namespace realm

#endif // REALM_GROUP_HPP
