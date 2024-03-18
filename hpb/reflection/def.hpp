// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google LLC.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef HPB_REFLECTION_DEF_HPP_
#define HPB_REFLECTION_DEF_HPP_

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "hpb/base/status.hpp"
#include "hpb/mem/arena.hpp"
#include "hpb/reflection/def.h"
#include "hpb/reflection/internal/def_pool.h"
#include "hpb/reflection/internal/enum_def.h"
#include "hpb/reflection/message.h"

// Must be last
#include "hpb/port/def.inc"

namespace hpb {

    typedef hpb_MessageValue MessageValue;

    class EnumDefPtr;

    class FileDefPtr;

    class MessageDefPtr;

    class OneofDefPtr;

    // A hpb::FieldDefPtr describes a single field in a message.  It is most often
    // found as a part of a hpb_MessageDef, but can also stand alone to represent
    // an extension.
    class FieldDefPtr {
    public:
        FieldDefPtr() : ptr_(nullptr) {}

        explicit FieldDefPtr(const hpb_FieldDef *ptr) : ptr_(ptr) {}

        const hpb_FieldDef *ptr() const { return ptr_; }

        typedef hpb_FieldType Type;
        typedef hpb_CType CType;
        typedef hpb_Label Label;

        FileDefPtr file() const;

        const char *full_name() const { return hpb_FieldDef_FullName(ptr_); }

        const hpb_MiniTableField *mini_table() const {
            return hpb_FieldDef_MiniTable(ptr_);
        }

        const HPB_DESC(FieldOptions) *options() const {
            return hpb_FieldDef_Options(ptr_);
        }

        Type type() const { return hpb_FieldDef_Type(ptr_); }

        CType ctype() const { return hpb_FieldDef_CType(ptr_); }

        Label label() const { return hpb_FieldDef_Label(ptr_); }

        const char *name() const { return hpb_FieldDef_Name(ptr_); }

        const char *json_name() const { return hpb_FieldDef_JsonName(ptr_); }

        uint32_t number() const { return hpb_FieldDef_Number(ptr_); }

        bool is_extension() const { return hpb_FieldDef_IsExtension(ptr_); }

        bool is_required() const { return hpb_FieldDef_IsRequired(ptr_); }

        bool has_presence() const { return hpb_FieldDef_HasPresence(ptr_); }

        // For non-string, non-submessage fields, this indicates whether binary
        // protobufs are encoded in packed or non-packed format.
        //
        // Note: this accessor reflects the fact that "packed" has different defaults
        // depending on whether the proto is proto2 or proto3.
        bool packed() const { return hpb_FieldDef_IsPacked(ptr_); }

        // An integer that can be used as an index into an array of fields for
        // whatever message this field belongs to.  Guaranteed to be less than
        // f->containing_type()->field_count().  May only be accessed once the def has
        // been finalized.
        uint32_t index() const { return hpb_FieldDef_Index(ptr_); }

        // The MessageDef to which this field belongs (for extensions, the extended
        // message).
        MessageDefPtr containing_type() const;

        // For extensions, the message the extension is declared inside, or NULL if
        // none.
        MessageDefPtr extension_scope() const;

        // The OneofDef to which this field belongs, or NULL if this field is not part
        // of a oneof.
        OneofDefPtr containing_oneof() const;

        OneofDefPtr real_containing_oneof() const;

        // Convenient field type tests.
        bool IsSubMessage() const { return hpb_FieldDef_IsSubMessage(ptr_); }

        bool IsString() const { return hpb_FieldDef_IsString(ptr_); }

        bool IsSequence() const { return hpb_FieldDef_IsRepeated(ptr_); }

        bool IsPrimitive() const { return hpb_FieldDef_IsPrimitive(ptr_); }

        bool IsMap() const { return hpb_FieldDef_IsMap(ptr_); }

        MessageValue default_value() const { return hpb_FieldDef_Default(ptr_); }

        // Returns the enum or submessage def for this field, if any.  The field's
        // type must match (ie. you may only call enum_subdef() for fields where
        // type() == kHpb_CType_Enum).
        EnumDefPtr enum_subdef() const;

        MessageDefPtr message_type() const;

        explicit operator bool() const { return ptr_ != nullptr; }

        friend bool operator==(FieldDefPtr lhs, FieldDefPtr rhs) {
            return lhs.ptr_ == rhs.ptr_;
        }

        friend bool operator!=(FieldDefPtr lhs, FieldDefPtr rhs) {
            return !(lhs == rhs);
        }

    private:
        const hpb_FieldDef *ptr_;
    };

    // Class that represents a oneof.
    class OneofDefPtr {
    public:
        OneofDefPtr() : ptr_(nullptr) {}

        explicit OneofDefPtr(const hpb_OneofDef *ptr) : ptr_(ptr) {}

        const hpb_OneofDef *ptr() const { return ptr_; }

        explicit operator bool() const { return ptr_ != nullptr; }

        const HPB_DESC(OneofOptions) *options() const {
            return hpb_OneofDef_Options(ptr_);
        }

        // Returns the MessageDef that contains this OneofDef.
        MessageDefPtr containing_type() const;

        // Returns the name of this oneof.
        const char *name() const { return hpb_OneofDef_Name(ptr_); }

        const char *full_name() const { return hpb_OneofDef_FullName(ptr_); }

        // Returns the number of fields in the oneof.
        int field_count() const { return hpb_OneofDef_FieldCount(ptr_); }

        FieldDefPtr field(int i) const {
            return FieldDefPtr(hpb_OneofDef_Field(ptr_, i));
        }

        // Looks up by name.
        FieldDefPtr FindFieldByName(const char *name, size_t len) const {
            return FieldDefPtr(hpb_OneofDef_LookupNameWithSize(ptr_, name, len));
        }

        FieldDefPtr FindFieldByName(const char *name) const {
            return FieldDefPtr(hpb_OneofDef_LookupName(ptr_, name));
        }

        template<class T>
        FieldDefPtr FindFieldByName(const T &str) const {
            return FindFieldByName(str.c_str(), str.size());
        }

        // Looks up by tag number.
        FieldDefPtr FindFieldByNumber(uint32_t num) const {
            return FieldDefPtr(hpb_OneofDef_LookupNumber(ptr_, num));
        }

    private:
        const hpb_OneofDef *ptr_;
    };

// Structure that describes a single .proto message type.
    class MessageDefPtr {
    public:
        MessageDefPtr() : ptr_(nullptr) {}

        explicit MessageDefPtr(const hpb_MessageDef *ptr) : ptr_(ptr) {}

        const HPB_DESC(MessageOptions) *options() const {
            return hpb_MessageDef_Options(ptr_);
        }

        std::string MiniDescriptorEncode() const {
            hpb::Arena arena;
            hpb_StringView md;
            hpb_MessageDef_MiniDescriptorEncode(ptr_, arena.ptr(), &md);
            return std::string(md.data, md.size);
        }

        const hpb_MessageDef *ptr() const { return ptr_; }

        FileDefPtr file() const;

        const char *full_name() const { return hpb_MessageDef_FullName(ptr_); }

        const char *name() const { return hpb_MessageDef_Name(ptr_); }

        const hpb_MiniTable *mini_table() const {
            return hpb_MessageDef_MiniTable(ptr_);
        }

        // The number of fields that belong to the MessageDef.
        int field_count() const { return hpb_MessageDef_FieldCount(ptr_); }

        FieldDefPtr field(int i) const {
            return FieldDefPtr(hpb_MessageDef_Field(ptr_, i));
        }

        // The number of oneofs that belong to the MessageDef.
        int oneof_count() const { return hpb_MessageDef_OneofCount(ptr_); }

        int real_oneof_count() const { return hpb_MessageDef_RealOneofCount(ptr_); }

        OneofDefPtr oneof(int i) const {
            return OneofDefPtr(hpb_MessageDef_Oneof(ptr_, i));
        }

        int enum_type_count() const { return hpb_MessageDef_NestedEnumCount(ptr_); }

        EnumDefPtr enum_type(int i) const;

        int nested_message_count() const {
            return hpb_MessageDef_NestedMessageCount(ptr_);
        }

        MessageDefPtr nested_message(int i) const {
            return MessageDefPtr(hpb_MessageDef_NestedMessage(ptr_, i));
        }

        int nested_extension_count() const {
            return hpb_MessageDef_NestedExtensionCount(ptr_);
        }

        FieldDefPtr nested_extension(int i) const {
            return FieldDefPtr(hpb_MessageDef_NestedExtension(ptr_, i));
        }

        int extension_range_count() const {
            return hpb_MessageDef_ExtensionRangeCount(ptr_);
        }

        hpb_Syntax syntax() const { return hpb_MessageDef_Syntax(ptr_); }

        // These return null pointers if the field is not found.
        FieldDefPtr FindFieldByNumber(uint32_t number) const {
            return FieldDefPtr(hpb_MessageDef_FindFieldByNumber(ptr_, number));
        }

        FieldDefPtr FindFieldByName(const char *name, size_t len) const {
            return FieldDefPtr(hpb_MessageDef_FindFieldByNameWithSize(ptr_, name, len));
        }

        FieldDefPtr FindFieldByName(const char *name) const {
            return FieldDefPtr(hpb_MessageDef_FindFieldByName(ptr_, name));
        }

        template<class T>
        FieldDefPtr FindFieldByName(const T &str) const {
            return FindFieldByName(str.c_str(), str.size());
        }

        OneofDefPtr FindOneofByName(const char *name, size_t len) const {
            return OneofDefPtr(hpb_MessageDef_FindOneofByNameWithSize(ptr_, name, len));
        }

        OneofDefPtr FindOneofByName(const char *name) const {
            return OneofDefPtr(hpb_MessageDef_FindOneofByName(ptr_, name));
        }

        template<class T>
        OneofDefPtr FindOneofByName(const T &str) const {
            return FindOneofByName(str.c_str(), str.size());
        }

        // Is this message a map entry?
        bool mapentry() const { return hpb_MessageDef_IsMapEntry(ptr_); }

        FieldDefPtr map_key() const {
            if (!mapentry()) return FieldDefPtr();
            return FieldDefPtr(hpb_MessageDef_Field(ptr_, 0));
        }

        FieldDefPtr map_value() const {
            if (!mapentry()) return FieldDefPtr();
            return FieldDefPtr(hpb_MessageDef_Field(ptr_, 1));
        }

        // Return the type of well known type message. kHpb_WellKnown_Unspecified for
        // non-well-known message.
        hpb_WellKnown wellknowntype() const {
            return hpb_MessageDef_WellKnownType(ptr_);
        }

        explicit operator bool() const { return ptr_ != nullptr; }

        friend bool operator==(MessageDefPtr lhs, MessageDefPtr rhs) {
            return lhs.ptr_ == rhs.ptr_;
        }

        friend bool operator!=(MessageDefPtr lhs, MessageDefPtr rhs) {
            return !(lhs == rhs);
        }

    private:
        class FieldIter {
        public:
            explicit FieldIter(const hpb_MessageDef *m, int i) : m_(m), i_(i) {}

            void operator++() { i_++; }

            FieldDefPtr operator*() {
                return FieldDefPtr(hpb_MessageDef_Field(m_, i_));
            }

            friend bool operator==(FieldIter lhs, FieldIter rhs) {
                return lhs.i_ == rhs.i_;
            }

            friend bool operator!=(FieldIter lhs, FieldIter rhs) {
                return !(lhs == rhs);
            }

        private:
            const hpb_MessageDef *m_;
            int i_;
        };

        class FieldAccessor {
        public:
            explicit FieldAccessor(const hpb_MessageDef *md) : md_(md) {}

            FieldIter begin() { return FieldIter(md_, 0); }

            FieldIter end() { return FieldIter(md_, hpb_MessageDef_FieldCount(md_)); }

        private:
            const hpb_MessageDef *md_;
        };

        class OneofIter {
        public:
            explicit OneofIter(const hpb_MessageDef *m, int i) : m_(m), i_(i) {}

            void operator++() { i_++; }

            OneofDefPtr operator*() {
                return OneofDefPtr(hpb_MessageDef_Oneof(m_, i_));
            }

            friend bool operator==(OneofIter lhs, OneofIter rhs) {
                return lhs.i_ == rhs.i_;
            }

            friend bool operator!=(OneofIter lhs, OneofIter rhs) {
                return !(lhs == rhs);
            }

        private:
            const hpb_MessageDef *m_;
            int i_;
        };

        class OneofAccessor {
        public:
            explicit OneofAccessor(const hpb_MessageDef *md) : md_(md) {}

            OneofIter begin() { return OneofIter(md_, 0); }

            OneofIter end() { return OneofIter(md_, hpb_MessageDef_OneofCount(md_)); }

        private:
            const hpb_MessageDef *md_;
        };

    public:
        FieldAccessor fields() const { return FieldAccessor(ptr()); }

        OneofAccessor oneofs() const { return OneofAccessor(ptr()); }

    private:
        const hpb_MessageDef *ptr_;
    };

    class EnumValDefPtr {
    public:
        EnumValDefPtr() : ptr_(nullptr) {}

        explicit EnumValDefPtr(const hpb_EnumValueDef *ptr) : ptr_(ptr) {}

        const HPB_DESC(EnumValueOptions) *options() const {
            return hpb_EnumValueDef_Options(ptr_);
        }

        int32_t number() const { return hpb_EnumValueDef_Number(ptr_); }

        const char *full_name() const { return hpb_EnumValueDef_FullName(ptr_); }

        const char *name() const { return hpb_EnumValueDef_Name(ptr_); }

    private:
        const hpb_EnumValueDef *ptr_;
    };

    class EnumDefPtr {
    public:
        EnumDefPtr() : ptr_(nullptr) {}

        explicit EnumDefPtr(const hpb_EnumDef *ptr) : ptr_(ptr) {}

        const HPB_DESC(EnumOptions) *options() const {
            return hpb_EnumDef_Options(ptr_);
        }

        const hpb_MiniTableEnum *mini_table() const {
            return _hpb_EnumDef_MiniTable(ptr_);
        }

        std::string MiniDescriptorEncode() const {
            hpb::Arena arena;
            hpb_StringView md;
            hpb_EnumDef_MiniDescriptorEncode(ptr_, arena.ptr(), &md);
            return std::string(md.data, md.size);
        }

        const hpb_EnumDef *ptr() const { return ptr_; }

        explicit operator bool() const { return ptr_ != nullptr; }

        const char *full_name() const { return hpb_EnumDef_FullName(ptr_); }

        const char *name() const { return hpb_EnumDef_Name(ptr_); }

        bool is_closed() const { return hpb_EnumDef_IsClosed(ptr_); }

        // The value that is used as the default when no field default is specified.
        // If not set explicitly, the first value that was added will be used.
        // The default value must be a member of the enum.
        // Requires that value_count() > 0.
        int32_t default_value() const { return hpb_EnumDef_Default(ptr_); }

        // Returns the number of values currently defined in the enum.  Note that
        // multiple names can refer to the same number, so this may be greater than
        // the total number of unique numbers.
        int value_count() const { return hpb_EnumDef_ValueCount(ptr_); }

        EnumValDefPtr value(int i) const {
            return EnumValDefPtr(hpb_EnumDef_Value(ptr_, i));
        }

        // Lookups from name to integer, returning true if found.
        EnumValDefPtr FindValueByName(const char *name) const {
            return EnumValDefPtr(hpb_EnumDef_FindValueByName(ptr_, name));
        }

        // Finds the name corresponding to the given number, or NULL if none was
        // found.  If more than one name corresponds to this number, returns the
        // first one that was added.
        EnumValDefPtr FindValueByNumber(int32_t num) const {
            return EnumValDefPtr(hpb_EnumDef_FindValueByNumber(ptr_, num));
        }

    private:
        const hpb_EnumDef *ptr_;
    };

    // Class that represents a .proto file with some things defined in it.
    //
    // Many users won't care about FileDefs, but they are necessary if you want to
    // read the values of file-level options.
    class FileDefPtr {
    public:
        explicit FileDefPtr(const hpb_FileDef *ptr) : ptr_(ptr) {}

        const HPB_DESC(FileOptions) *options() const {
            return hpb_FileDef_Options(ptr_);
        }

        const hpb_FileDef *ptr() const { return ptr_; }

        // Get/set name of the file (eg. "foo/bar.proto").
        const char *name() const { return hpb_FileDef_Name(ptr_); }

        // Package name for definitions inside the file (eg. "foo.bar").
        const char *package() const { return hpb_FileDef_Package(ptr_); }

        // Syntax for the file.  Defaults to proto2.
        hpb_Syntax syntax() const { return hpb_FileDef_Syntax(ptr_); }

        // Get the list of dependencies from the file.  These are returned in the
        // order that they were added to the FileDefPtr.
        int dependency_count() const { return hpb_FileDef_DependencyCount(ptr_); }

        FileDefPtr dependency(int index) const {
            return FileDefPtr(hpb_FileDef_Dependency(ptr_, index));
        }

        int public_dependency_count() const {
            return hpb_FileDef_PublicDependencyCount(ptr_);
        }

        FileDefPtr public_dependency(int index) const {
            return FileDefPtr(hpb_FileDef_PublicDependency(ptr_, index));
        }

        int toplevel_enum_count() const {
            return hpb_FileDef_TopLevelEnumCount(ptr_);
        }

        EnumDefPtr toplevel_enum(int index) const {
            return EnumDefPtr(hpb_FileDef_TopLevelEnum(ptr_, index));
        }

        int toplevel_message_count() const {
            return hpb_FileDef_TopLevelMessageCount(ptr_);
        }

        MessageDefPtr toplevel_message(int index) const {
            return MessageDefPtr(hpb_FileDef_TopLevelMessage(ptr_, index));
        }

        int toplevel_extension_count() const {
            return hpb_FileDef_TopLevelExtensionCount(ptr_);
        }

        FieldDefPtr toplevel_extension(int index) const {
            return FieldDefPtr(hpb_FileDef_TopLevelExtension(ptr_, index));
        }

        explicit operator bool() const { return ptr_ != nullptr; }

        friend bool operator==(FileDefPtr lhs, FileDefPtr rhs) {
            return lhs.ptr_ == rhs.ptr_;
        }

        friend bool operator!=(FileDefPtr lhs, FileDefPtr rhs) {
            return !(lhs == rhs);
        }

    private:
        const hpb_FileDef *ptr_;
    };

    // Non-const methods in hpb::DefPool are NOT thread-safe.
    class DefPool {
    public:
        DefPool() : ptr_(hpb_DefPool_New(), hpb_DefPool_Free) {}

        explicit DefPool(hpb_DefPool *s) : ptr_(s, hpb_DefPool_Free) {}

        const hpb_DefPool *ptr() const { return ptr_.get(); }

        hpb_DefPool *ptr() { return ptr_.get(); }

        // Finds an entry in the symbol table with this exact name.  If not found,
        // returns NULL.
        MessageDefPtr FindMessageByName(const char *sym) const {
            return MessageDefPtr(hpb_DefPool_FindMessageByName(ptr_.get(), sym));
        }

        EnumDefPtr FindEnumByName(const char *sym) const {
            return EnumDefPtr(hpb_DefPool_FindEnumByName(ptr_.get(), sym));
        }

        FileDefPtr FindFileByName(const char *name) const {
            return FileDefPtr(hpb_DefPool_FindFileByName(ptr_.get(), name));
        }

        FieldDefPtr FindExtensionByName(const char *name) const {
            return FieldDefPtr(hpb_DefPool_FindExtensionByName(ptr_.get(), name));
        }

        void _SetPlatform(hpb_MiniTablePlatform platform) {
            _hpb_DefPool_SetPlatform(ptr_.get(), platform);
        }

        // TODO: iteration?

        // Adds the given serialized FileDescriptorProto to the pool.
        FileDefPtr AddFile(const HPB_DESC(FileDescriptorProto) *file_proto,
                           Status *status) {
            return FileDefPtr(
                    hpb_DefPool_AddFile(ptr_.get(), file_proto, status->ptr()));
        }

    private:
        std::unique_ptr<hpb_DefPool, decltype(&hpb_DefPool_Free)> ptr_;
    };

    // TODO(b/236632406): This typedef is deprecated. Delete it.
    using SymbolTable = DefPool;

    inline FileDefPtr FieldDefPtr::file() const {
        return FileDefPtr(hpb_FieldDef_File(ptr_));
    }

    inline FileDefPtr MessageDefPtr::file() const {
        return FileDefPtr(hpb_MessageDef_File(ptr_));
    }

    inline EnumDefPtr MessageDefPtr::enum_type(int i) const {
        return EnumDefPtr(hpb_MessageDef_NestedEnum(ptr_, i));
    }

    inline MessageDefPtr FieldDefPtr::message_type() const {
        return MessageDefPtr(hpb_FieldDef_MessageSubDef(ptr_));
    }

    inline MessageDefPtr FieldDefPtr::containing_type() const {
        return MessageDefPtr(hpb_FieldDef_ContainingType(ptr_));
    }

    inline MessageDefPtr FieldDefPtr::extension_scope() const {
        return MessageDefPtr(hpb_FieldDef_ExtensionScope(ptr_));
    }

    inline MessageDefPtr OneofDefPtr::containing_type() const {
        return MessageDefPtr(hpb_OneofDef_ContainingType(ptr_));
    }

    inline OneofDefPtr FieldDefPtr::containing_oneof() const {
        return OneofDefPtr(hpb_FieldDef_ContainingOneof(ptr_));
    }

    inline OneofDefPtr FieldDefPtr::real_containing_oneof() const {
        return OneofDefPtr(hpb_FieldDef_RealContainingOneof(ptr_));
    }

    inline EnumDefPtr FieldDefPtr::enum_subdef() const {
        return EnumDefPtr(hpb_FieldDef_EnumSubDef(ptr_));
    }

}  // namespace hpb

#include "hpb/port/undef.inc"

#endif  // HPB_REFLECTION_DEF_HPP_
