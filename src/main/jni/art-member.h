//
// Created by asus on 2016/8/8.
//

#ifndef HOOKMANAGER_ART_METHOD_H
#define HOOKMANAGER_ART_METHOD_H

#include "globals.h"
#include "sys/cdefs.h"
#include "base/macros.h"

#define MANAGED PACKED(4)
class MANAGED ObjectReference{
    uint32_t reference_;
};
class MANAGED CompressedReference : public ObjectReference{};
class  MANAGED GcRoot{
    mutable CompressedReference root_;
};
class ArtField final{
public:
    GcRoot declaring_class_;

    uint32_t access_flags_;

    // Dex cache index of field id
    uint32_t field_dex_idx_;

    // Offset of field within an instance or in the Class' static fields
    uint32_t offset_;
};
class ArtFieldKitkat final{
public:
    void* declaring_class_;

    uint32_t access_flags_;

    // Dex cache index of field id
    uint32_t field_dex_idx_;

    // Offset of field within an instance or in the Class' static fields
    uint32_t offset_;
};
class ArtMethodLollipop final{
public:
// Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
    // The class we are a part of
    uint32_t declaring_class_;

    // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
    uint32_t dex_cache_resolved_methods_;

    // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
    uint32_t dex_cache_resolved_types_;

    // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
    uint32_t dex_cache_strings_;

    // Method dispatch from the interpreter invokes this pointer which may cause a bridge into
    // compiled code.
    uint64_t entry_point_from_interpreter_;

    // Pointer to JNI function registered to this method, or a function to resolve the JNI function.
    uint64_t entry_point_from_jni_;

    // Method dispatch from portable compiled code invokes this pointer which may cause bridging into
    // quick compiled code or the interpreter.
#ifdef ART_USE_PORTABLE_COMPILER
    uint64_t entry_point_from_portable_compiled_code_;
#endif

    // Method dispatch from quick compiled code invokes this pointer which may cause bridging into
    // portable compiled code or the interpreter.
    uint64_t entry_point_from_quick_compiled_code_;

    // Pointer to a data structure created by the compiler and used by the garbage collector to
    // determine which registers hold live references to objects within the heap. Keyed by native PC
    // offsets for the quick compiler and dex PCs for the portable.
    uint64_t gc_map_;

    // Access flags; low 16 bits are defined by spec.
    uint32_t access_flags_;

    /* Dex file fields. The defining dex file is available via declaring_class_->dex_cache_ */

    // Offset to the CodeItem.
    uint32_t dex_code_item_offset_;

    // Index into method_ids of the dex file associated with this method.
    uint32_t dex_method_index_;

    /* End of dex file fields. */

    // Entry within a dispatch table for this method. For static/direct methods the index is into
    // the declaringClass.directMethods, for virtual methods the vtable and for interface methods the
    // ifTable.
    uint32_t method_index_;
};
class ArtMethodKitkat{
public:
    // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
    // The class we are a part of
    void* declaring_class_;
    // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
    void* dex_cache_initialized_static_storage_;
    // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
    void* dex_cache_resolved_methods_;
    // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
    void* dex_cache_resolved_types_;
    // short cuts to declaring_class_->dex_cache_ member for fast compiled code access
    void* dex_cache_strings_;
    // Access flags; low 16 bits are defined by spec.
    uint32_t access_flags_;
    // Offset to the CodeItem.
    uint32_t dex_code_item_offset_;
    // Architecture-dependent register spill mask
    uint32_t core_spill_mask_;
    // Compiled code associated with this method for callers from managed code.
    // May be compiled managed code or a bridge for invoking a native method.
    // TODO: Break apart this into portable and quick.
    const void* entry_point_from_compiled_code_;
    // Called by the interpreter to execute this method.
    void* entry_point_from_interpreter_;
    // Architecture-dependent register spill mask
    uint32_t fp_spill_mask_;
    // Total size in bytes of the frame
    size_t frame_size_in_bytes_;
    // Garbage collection map of native PC offsets (quick) or dex PCs (portable) to reference bitmaps.
    const uint8_t* gc_map_;
    // Mapping from native pc to dex pc
    const uint32_t* mapping_table_;
    // Index into method_ids of the dex file associated with this method
    uint32_t method_dex_index_;
    // For concrete virtual methods, this is the offset of the method in Class::vtable_.
    //
    // For abstract methods in an interface class, this is the offset of the method in
    // "iftable_->Get(n)->GetMethodArray()".
    //
    // For static and direct methods this is the index in the direct methods table.
    uint32_t method_index_;

    //something leftOver
};
class ArtMethod final{
public:
    GcRoot declaring_class_;

    // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
    GcRoot dex_cache_resolved_methods_;

    // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
    GcRoot dex_cache_resolved_types_;

    // Access flags; low 16 bits are defined by spec.
    uint32_t access_flags_;

    /* Dex file fields. The defining dex file is available via declaring_class_->dex_cache_ */

    // Offset to the CodeItem.
    uint32_t dex_code_item_offset_;

    // Index into method_ids of the dex file associated with this method.
    uint32_t dex_method_index_;

    /* End of dex file fields. */

    // Entry within a dispatch table for this method. For static/direct methods the index is into
    // the declaringClass.directMethods, for virtual methods the vtable and for interface methods the
    // ifTable.
    uint32_t method_index_;

    //something left over
};
class ArtMethodNougat final{
public:

    // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
    // The class we are a part of.
    uint32_t declaring_class_;
    // Access flags; low 16 bits are defined by spec.
    uint32_t access_flags_;
    /* Dex file fields. The defining dex file is available via declaring_class_->dex_cache_ */
    // Offset to the CodeItem.
    uint32_t dex_code_item_offset_;
    // Index into method_ids of the dex file associated with this method.
    uint32_t dex_method_index_;
    /* End of dex file fields. */
    // Entry within a dispatch table for this method. For static/direct methods the index is into
    // the declaringClass.directMethods, for virtual methods the vtable and for interface methods the
    // ifTable.
    uint16_t method_index_;

    //something left over
};

#endif //HOOKMANAGER_ART_METHOD_H
