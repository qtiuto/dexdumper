/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_DEX_FILE_H_
#define ART_RUNTIME_DEX_FILE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "globals.h"
#include "invoke_type.h"
#include "jni.h"
#include "modifiers.h"

extern void skipULeb128(const uint8_t *&ptr);
namespace art {
    class MemMap {
    public:
        const std::string name_;
        byte *const begin_;  // Start of data.
        size_t size_;  // Length of data.

        void *const base_begin_;  // Page-aligned base address.
        size_t base_size_;  // Length of mapping. May be changed by RemapAtEnd (ie Zygote).
        int prot_;  // Protection of the map.

        // When reuse_ is true, this is just a view of an existing mapping
        // and we do not take ownership and are not responsible for
        // unmapping.
        const bool reuse_;
    };

// TODO: remove dependencies on mirror classes, primarily by moving
// EncodedStaticFieldValueIterator to its own file.
// TODO: move all of the macro functionality into the DexCache class.
    class DexFile {
    public:
        static const byte kDexMagic[];
        static const byte kDexMagicVersion[];
        static const size_t kSha1DigestSize = 20;
        static const uint32_t kDexEndianConstant = 0x12345678;

        // name of the DexFile entry within a zip archive
        static const char *kClassesDex;

        // The value of an invalid index.
        static const uint32_t kDexNoIndex = 0xFFFFFFFF;

        // The value of an invalid index.
        static const uint16_t kDexNoIndex16 = 0xFFFF;

        // The separator charactor in MultiDex locations.
        static constexpr char kMultiDexSeparator = ':';

        // A string version of the previous. This is a define so that we can merge string literals in the
        // preprocessor.
#define kMultiDexSeparatorString ":"

        // Raw header_item.
        struct Header {
            uint8_t magic_[8];
            uint32_t checksum_;  // See also location_checksum_
            uint8_t signature_[kSha1DigestSize];
            uint32_t file_size_;  // size of entire file
            uint32_t header_size_;  // offset to start of next section
            uint32_t endian_tag_;
            uint32_t link_size_;  // unused
            uint32_t link_off_;  // unused
            uint32_t map_off_;  // unused
            uint32_t string_ids_size_;  // number of StringIds
            uint32_t string_ids_off_;  // file offset of StringIds array
            uint32_t type_ids_size_;  // number of TypeIds, we don't support more than 65535
            uint32_t type_ids_off_;  // file offset of TypeIds array
            uint32_t proto_ids_size_;  // number of ProtoIds, we don't support more than 65535
            uint32_t proto_ids_off_;  // file offset of ProtoIds array
            uint32_t field_ids_size_;  // number of FieldIds
            uint32_t field_ids_off_;  // file offset of FieldIds array
            uint32_t method_ids_size_;  // number of MethodIds
            uint32_t method_ids_off_;  // file offset of MethodIds array
            uint32_t class_defs_size_;  // number of ClassDefs
            uint32_t class_defs_off_;  // file offset of ClassDef array
            uint32_t data_size_;  // unused
            uint32_t data_off_;  // unused
        };

        // Map item type codes.
        enum {
            kDexTypeHeaderItem = 0x0000,
            kDexTypeStringIdItem = 0x0001,
            kDexTypeTypeIdItem = 0x0002,
            kDexTypeProtoIdItem = 0x0003,
            kDexTypeFieldIdItem = 0x0004,
            kDexTypeMethodIdItem = 0x0005,
            kDexTypeClassDefItem = 0x0006,
            kDexTypeMapList = 0x1000,
            kDexTypeTypeList = 0x1001,
            kDexTypeAnnotationSetRefList = 0x1002,
            kDexTypeAnnotationSetItem = 0x1003,
            kDexTypeClassDataItem = 0x2000,
            kDexTypeCodeItem = 0x2001,
            kDexTypeStringDataItem = 0x2002,
            kDexTypeDebugInfoItem = 0x2003,
            kDexTypeAnnotationItem = 0x2004,
            kDexTypeEncodedArrayItem = 0x2005,
            kDexTypeAnnotationsDirectoryItem = 0x2006,
        };

        struct MapItem {
            uint16_t type_;
            uint16_t unused_;
            uint32_t size_;
            uint32_t offset_;

        };

        struct MapList {
            uint32_t size_;
            MapItem list_[1];

        };

        // Raw string_id_item.
        struct StringId {
            uint32_t string_data_off_;  // offset in bytes from the base address
        };

        // Raw type_id_item.
        struct TypeId {
            uint32_t descriptor_idx_;  // index into string_ids
        };

        // Raw field_id_item.
        struct FieldId {
            uint16_t class_idx_;  // index into type_ids_ array for defining class
            uint16_t type_idx_;  // index into type_ids_ array for field type
            uint32_t name_idx_;  // index into string_ids_ array for field name

        private:

        };

        // Raw method_id_item.
        struct MethodId {
            uint16_t class_idx_;  // index into type_ids_ array for defining class
            uint16_t proto_idx_;  // index into proto_ids_ array for method prototype
            uint32_t name_idx_;  // index into string_ids_ array for method name
        };

        // Raw proto_id_item.
        struct ProtoId {
            uint32_t shorty_idx_;  // index into string_ids array for shorty descriptor
            uint16_t return_type_idx_;  // index into type_ids array for return type
            uint16_t pad_;             // padding = 0
            uint32_t parameters_off_;  // file offset to type_list for parameter types
        };

        // Raw class_def_item.
        struct ClassDef {
            uint16_t class_idx_;  // index into type_ids_ array for this class
            uint16_t pad1_;  // padding = 0
            uint32_t access_flags_;
            uint16_t superclass_idx_;  // index into type_ids_ array for superclass
            uint16_t pad2_;  // padding = 0
            uint32_t interfaces_off_;  // file offset to TypeList
            uint32_t source_file_idx_;  // index into string_ids_ for source file name
            uint32_t annotations_off_;  // file offset to annotations_directory_item
            uint32_t class_data_off_;  // file offset to class_data_item
            uint32_t static_values_off_;  // file offset to EncodedArray

            // Returns the valid access flags, that is, Java modifier bits relevant to the ClassDef type
            // (class or interface). These are all in the lower 16b and do not contain runtime flags.
            uint32_t GetJavaAccessFlags() const {
                if ((access_flags_ & kAccInterface) != 0) {
                    // Interface.
                    return access_flags_ & kAccValidInterfaceFlags;
                } else {
                    // Class.
                    return access_flags_ & kAccValidClassFlags;
                }
            }


        };

        // Raw type_item.
        struct TypeItem {
            uint16_t type_idx_;  // index into type_ids section
        };

        // Raw type_list.
        class TypeList {
        public:
            uint32_t Size() const {
                return size_;
            }

            const TypeItem &GetTypeItem(uint32_t idx) const {
                return this->list_[idx];
            }

            // Size in bytes of the part of the list that is common.
            static constexpr size_t GetHeaderSize() {
                return 4U;
            }

            // Size in bytes of the whole type list including all the stored elements.
            static constexpr size_t GetListSize(size_t count) {
                return GetHeaderSize() + sizeof(TypeItem) * count;
            }

        private:
            uint32_t size_;  // size of the list, in entries
            TypeItem list_[1];  // elements of the list

        };

        // Raw code_item.
        struct CodeItem {
            uint16_t registers_size_;
            uint16_t ins_size_;
            uint16_t outs_size_;
            uint16_t tries_size_;
            uint32_t debug_info_off_;  // file offset to debug info stream
            uint32_t insns_size_in_code_units_;  // size of the insns array, in 2 byte code units
            uint16_t insns_[1];
        }__attribute__ ((packed));//to avoid default 4-byte aligned imposed by the complier,which cause sizeof return 20 rather than true size 18
        // Raw try_item.
        struct TryItem {
            uint32_t start_addr_;
            uint16_t insn_count_;
            uint16_t handler_off_;
        };


        // Annotation constants.
        enum {
            kDexVisibilityBuild = 0x00, /* annotation visibility */
                    kDexVisibilityRuntime = 0x01,
            kDexVisibilitySystem = 0x02,

            kDexAnnotationByte = 0x00,
            kDexAnnotationShort = 0x02,
            kDexAnnotationChar = 0x03,
            kDexAnnotationInt = 0x04,
            kDexAnnotationLong = 0x06,
            kDexAnnotationFloat = 0x10,
            kDexAnnotationDouble = 0x11,
            kDexAnnotationString = 0x17,
            kDexAnnotationType = 0x18,
            kDexAnnotationField = 0x19,
            kDexAnnotationMethod = 0x1a,
            kDexAnnotationEnum = 0x1b,
            kDexAnnotationArray = 0x1c,
            kDexAnnotationAnnotation = 0x1d,
            kDexAnnotationNull = 0x1e,
            kDexAnnotationBoolean = 0x1f,

            kDexAnnotationValueTypeMask = 0x1f, /* low 5 bits */
                    kDexAnnotationValueArgShift = 5,
        };

        struct AnnotationsDirectoryItem {
            uint32_t class_annotations_off_;
            uint32_t fields_size_;
            uint32_t methods_size_;
            uint32_t parameters_size_;
        };

        struct FieldAnnotationsItem {
            uint32_t field_idx_;
            uint32_t annotations_off_;
        };

        struct MethodAnnotationsItem {
            uint32_t method_idx_;
            uint32_t annotations_off_;
        };

        struct ParameterAnnotationsItem {
            uint32_t method_idx_;
            uint32_t annotations_off_;
        };

        struct AnnotationSetRefItem {
            uint32_t annotations_off_;
        };

        struct AnnotationSetRefList {
            uint32_t size_;
            AnnotationSetRefItem list_[1];
        };

        struct AnnotationSetItem {
            uint32_t size_;
            uint32_t entries_[1];
        };

        struct AnnotationItem {
            uint8_t visibility_;
            uint8_t annotation_[1];
        };


        // Do nothing
        virtual ~DexFile() { }


        // Callback for "new position table entry".
        // Returning true causes the decoder to stop early.
        typedef bool (*DexDebugNewPositionCb)(void *context, uint32_t address, uint32_t line_num);

        // Debug info opcodes and constants
        enum {
            DBG_END_SEQUENCE = 0x00,
            DBG_ADVANCE_PC = 0x01,
            DBG_ADVANCE_LINE = 0x02,
            DBG_START_LOCAL = 0x03,
            DBG_START_LOCAL_EXTENDED = 0x04,
            DBG_END_LOCAL = 0x05,
            DBG_RESTART_LOCAL = 0x06,
            DBG_SET_PROLOGUE_END = 0x07,
            DBG_SET_EPILOGUE_BEGIN = 0x08,
            DBG_SET_FILE = 0x09,
            DBG_FIRST_SPECIAL = 0x0a,
            DBG_LINE_BASE = -4,
            DBG_LINE_RANGE = 15,
        };

        struct LocalInfo {
            LocalInfo()
                    : name_(NULL), descriptor_(NULL), signature_(NULL), start_address_(0),
                      is_live_(false) { }

            const char *name_;  // E.g., list
            const char *descriptor_;  // E.g., Ljava/util/LinkedList;
            const char *signature_;  // E.g., java.util.LinkedList<java.lang.Integer>
            uint16_t start_address_;  // PC location where the local is first defined.
            bool is_live_;  // Is the local defined and live.

        };

        struct UTF16EmptyFn {
            void MakeEmpty(std::pair<const char *, const ClassDef *> &pair) const {
                pair.first = nullptr;
                pair.second = nullptr;
            }

            bool IsEmpty(const std::pair<const char *, const ClassDef *> &pair) const {
                if (pair.first == nullptr) {
                    return true;
                }
                return false;
            }
        };

        struct UTF16HashCmp {
            // Hash function.
            size_t operator()(const char *key) const {
                return 0;
            }

            // std::equal function.
            bool operator()(const char *a, const char *b) const {
                return false;
            }
        };


        //private:

        enum class ZipOpenErrorCode {  // private
            kNoError,
            kEntryNotFound,
            kExtractToMemoryError,
            kDexFileError,
            kMakeReadOnlyError,
            kVerifyError
        };


        // The base address of the memory mapping.
        const byte * begin_;

        // The size of the underlying memory allocation in bytes.
        const size_t size_;

        // Typically the dex file name when available, alternatively some identifying string.
        //
        // The ClassLinker will use this to match DexFiles the boot class
        // path to DexCache::GetLocation when loading from an image.
        const std::string location_;

        const uint32_t location_checksum_;

        // Manages the underlying memory allocation.
        std::unique_ptr <MemMap> mem_map_;

        // Points to the header section.
        const Header *const header_;

        // Points to the base of the string identifier list.
        const StringId *const string_ids_;

        // Points to the base of the type identifier list.
        const TypeId *const type_ids_;

        // Points to the base of the field identifier list.
        const FieldId *const field_ids_;

        // Points to the base of the method identifier list.
        const MethodId *const method_ids_;

        // Points to the base of the prototype identifier list.
        const ProtoId *const proto_ids_;

        // Points to the base of the class definition list.
        const ClassDef *const class_defs_;

        // Number of misses finding a class def from a descriptor.
        mutable std::atomic <uint32_t> find_class_def_misses_;

        DexFile(const byte* beg,const Header *const header,
                const StringId *const string_ids,const TypeId *const type_ids,
                const FieldId *const field_ids,const MethodId *const method_ids,
                const ProtoId *const proto_ids,const ClassDef *const class_defs):
                begin_(beg),size_(0),location_checksum_(0),location_(),header_(header),string_ids_(string_ids),type_ids_(type_ids),
                field_ids_(field_ids),method_ids_(method_ids),proto_ids_(proto_ids),class_defs_(class_defs){}
        const char * getStringByStringIndex(const uint32_t index)const {
            const byte * ptr= begin_ + string_ids_[index].string_data_off_;
            skipULeb128(ptr);
            return (const char *) ptr;
        }
        const char * getStringFromTypeIndex(u4 index)const {
            return getStringByStringIndex(type_ids_[index].descriptor_idx_);
        }
    private:
        DexFile():begin_(0),size_(0),location_(),location_checksum_(0),header_(nullptr),proto_ids_(
                nullptr),method_ids_(nullptr),string_ids_(nullptr),type_ids_(nullptr),field_ids_(
                nullptr),class_defs_(nullptr){}
    };
}

//
#endif  // ART_RUNTIME_DEX_FILE_H_

