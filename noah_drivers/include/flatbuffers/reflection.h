/*
 * Copyright 2015 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FLATBUFFERS_REFLECTION_H_
#define FLATBUFFERS_REFLECTION_H_

// This is somewhat of a circular dependency because flatc (and thus this
// file) is needed to generate this header in the first place.
// Should normally not be a problem since it can be generated by the
// previous version of flatc whenever this code needs to change.
// See reflection/generate_code.sh
#include <flatbuffers/reflection_generated.h>

// Helper functionality for reflection.

namespace flatbuffers {

// ------------------------- GETTERS -------------------------

inline bool IsScalar(reflection::BaseType t) {
  return t >= reflection::UType && t <= reflection::Double;
}
inline bool IsInteger(reflection::BaseType t) {
  return t >= reflection::UType && t <= reflection::ULong;
}
inline bool IsFloat(reflection::BaseType t) {
  return t == reflection::Float || t == reflection::Double;
}
inline bool IsLong(reflection::BaseType t) {
  return t == reflection::Long || t == reflection::ULong;
}

// Size of a basic type, don't use with structs.
inline size_t GetTypeSize(reflection::BaseType base_type) {
  // This needs to correspond to the BaseType enum.
  static size_t sizes[] = { 0, 1, 1, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 4, 4, 4, 4 };
  return sizes[base_type];
}

// Same as above, but now correctly returns the size of a struct if
// the field (or vector element) is a struct.
inline size_t GetTypeSizeInline(reflection::BaseType base_type, int type_index,
                                const reflection::Schema &schema) {
  if (base_type == reflection::Obj &&
      schema.objects()->Get(type_index)->is_struct()) {
    return schema.objects()->Get(type_index)->bytesize();
  } else {
    return GetTypeSize(base_type);
  }
}

// Get the root, regardless of what type it is.
inline Table *GetAnyRoot(uint8_t *flatbuf) {
  return GetMutableRoot<Table>(flatbuf);
}
inline const Table *GetAnyRoot(const uint8_t *flatbuf) {
  return GetRoot<Table>(flatbuf);
}

// Get a field's default, if you know it's an integer, and its exact type.
template<typename T> T GetFieldDefaultI(const reflection::Field &field) {
  FLATBUFFERS_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
  return static_cast<T>(field.default_integer());
}

// Get a field's default, if you know it's floating point and its exact type.
template<typename T> T GetFieldDefaultF(const reflection::Field &field) {
  FLATBUFFERS_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
  return static_cast<T>(field.default_real());
}

// Get a field, if you know it's an integer, and its exact type.
template<typename T>
T GetFieldI(const Table &table, const reflection::Field &field) {
  FLATBUFFERS_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
  return table.GetField<T>(field.offset(),
                           static_cast<T>(field.default_integer()));
}

// Get a field, if you know it's floating point and its exact type.
template<typename T>
T GetFieldF(const Table &table, const reflection::Field &field) {
  FLATBUFFERS_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
  return table.GetField<T>(field.offset(),
                           static_cast<T>(field.default_real()));
}

// Get a field, if you know it's a string.
inline const String *GetFieldS(const Table &table,
                               const reflection::Field &field) {
  FLATBUFFERS_ASSERT(field.type()->base_type() == reflection::String);
  return table.GetPointer<const String *>(field.offset());
}

// Get a field, if you know it's a vector.
template<typename T>
Vector<T> *GetFieldV(const Table &table, const reflection::Field &field) {
  FLATBUFFERS_ASSERT(field.type()->base_type() == reflection::Vector &&
                     sizeof(T) == GetTypeSize(field.type()->element()));
  return table.GetPointer<Vector<T> *>(field.offset());
}

// Get a field, if you know it's a vector, generically.
// To actually access elements, use the return value together with
// field.type()->element() in any of GetAnyVectorElemI below etc.
inline VectorOfAny *GetFieldAnyV(const Table &table,
                                 const reflection::Field &field) {
  return table.GetPointer<VectorOfAny *>(field.offset());
}

// Get a field, if you know it's a table.
inline Table *GetFieldT(const Table &table, const reflection::Field &field) {
  FLATBUFFERS_ASSERT(field.type()->base_type() == reflection::Obj ||
                     field.type()->base_type() == reflection::Union);
  return table.GetPointer<Table *>(field.offset());
}

// Get a field, if you know it's a struct.
inline const Struct *GetFieldStruct(const Table &table,
                                    const reflection::Field &field) {
  // TODO: This does NOT check if the field is a table or struct, but we'd need
  // access to the schema to check the is_struct flag.
  FLATBUFFERS_ASSERT(field.type()->base_type() == reflection::Obj);
  return table.GetStruct<const Struct *>(field.offset());
}

// Get a structure's field, if you know it's a struct.
inline const Struct *GetFieldStruct(const Struct &structure,
                                    const reflection::Field &field) {
  FLATBUFFERS_ASSERT(field.type()->base_type() == reflection::Obj);
  return structure.GetStruct<const Struct *>(field.offset());
}

// Raw helper functions used below: get any value in memory as a 64bit int, a
// double or a string.
// All scalars get static_cast to an int64_t, strings use strtoull, every other
// data type returns 0.
int64_t GetAnyValueI(reflection::BaseType type, const uint8_t *data);
// All scalars static cast to double, strings use strtod, every other data
// type is 0.0.
double GetAnyValueF(reflection::BaseType type, const uint8_t *data);
// All scalars converted using stringstream, strings as-is, and all other
// data types provide some level of debug-pretty-printing.
std::string GetAnyValueS(reflection::BaseType type, const uint8_t *data,
                         const reflection::Schema *schema, int type_index);

// Get any table field as a 64bit int, regardless of what type it is.
inline int64_t GetAnyFieldI(const Table &table,
                            const reflection::Field &field) {
  auto field_ptr = table.GetAddressOf(field.offset());
  return field_ptr ? GetAnyValueI(field.type()->base_type(), field_ptr)
                   : field.default_integer();
}

// Get any table field as a double, regardless of what type it is.
inline double GetAnyFieldF(const Table &table, const reflection::Field &field) {
  auto field_ptr = table.GetAddressOf(field.offset());
  return field_ptr ? GetAnyValueF(field.type()->base_type(), field_ptr)
                   : field.default_real();
}

// Get any table field as a string, regardless of what type it is.
// You may pass nullptr for the schema if you don't care to have fields that
// are of table type pretty-printed.
inline std::string GetAnyFieldS(const Table &table,
                                const reflection::Field &field,
                                const reflection::Schema *schema) {
  auto field_ptr = table.GetAddressOf(field.offset());
  return field_ptr ? GetAnyValueS(field.type()->base_type(), field_ptr, schema,
                                  field.type()->index())
                   : "";
}

// Get any struct field as a 64bit int, regardless of what type it is.
inline int64_t GetAnyFieldI(const Struct &st, const reflection::Field &field) {
  return GetAnyValueI(field.type()->base_type(),
                      st.GetAddressOf(field.offset()));
}

// Get any struct field as a double, regardless of what type it is.
inline double GetAnyFieldF(const Struct &st, const reflection::Field &field) {
  return GetAnyValueF(field.type()->base_type(),
                      st.GetAddressOf(field.offset()));
}

// Get any struct field as a string, regardless of what type it is.
inline std::string GetAnyFieldS(const Struct &st,
                                const reflection::Field &field) {
  return GetAnyValueS(field.type()->base_type(),
                      st.GetAddressOf(field.offset()), nullptr, -1);
}

// Get any vector element as a 64bit int, regardless of what type it is.
inline int64_t GetAnyVectorElemI(const VectorOfAny *vec,
                                 reflection::BaseType elem_type, size_t i) {
  return GetAnyValueI(elem_type, vec->Data() + GetTypeSize(elem_type) * i);
}

// Get any vector element as a double, regardless of what type it is.
inline double GetAnyVectorElemF(const VectorOfAny *vec,
                                reflection::BaseType elem_type, size_t i) {
  return GetAnyValueF(elem_type, vec->Data() + GetTypeSize(elem_type) * i);
}

// Get any vector element as a string, regardless of what type it is.
inline std::string GetAnyVectorElemS(const VectorOfAny *vec,
                                     reflection::BaseType elem_type, size_t i) {
  return GetAnyValueS(elem_type, vec->Data() + GetTypeSize(elem_type) * i,
                      nullptr, -1);
}

// Get a vector element that's a table/string/vector from a generic vector.
// Pass Table/String/VectorOfAny as template parameter.
// Warning: does no typechecking.
template<typename T>
T *GetAnyVectorElemPointer(const VectorOfAny *vec, size_t i) {
  auto elem_ptr = vec->Data() + sizeof(uoffset_t) * i;
  return reinterpret_cast<T *>(elem_ptr + ReadScalar<uoffset_t>(elem_ptr));
}

// Get the inline-address of a vector element. Useful for Structs (pass Struct
// as template arg), or being able to address a range of scalars in-line.
// Get elem_size from GetTypeSizeInline().
// Note: little-endian data on all platforms, use EndianScalar() instead of
// raw pointer access with scalars).
template<typename T>
T *GetAnyVectorElemAddressOf(const VectorOfAny *vec, size_t i,
                             size_t elem_size) {
  return reinterpret_cast<T *>(vec->Data() + elem_size * i);
}

// Similarly, for elements of tables.
template<typename T>
T *GetAnyFieldAddressOf(const Table &table, const reflection::Field &field) {
  return reinterpret_cast<T *>(table.GetAddressOf(field.offset()));
}

// Similarly, for elements of structs.
template<typename T>
T *GetAnyFieldAddressOf(const Struct &st, const reflection::Field &field) {
  return reinterpret_cast<T *>(st.GetAddressOf(field.offset()));
}

// ------------------------- SETTERS -------------------------

// Set any scalar field, if you know its exact type.
template<typename T>
bool SetField(Table *table, const reflection::Field &field, T val) {
  reflection::BaseType type = field.type()->base_type();
  if (!IsScalar(type)) { return false; }
  FLATBUFFERS_ASSERT(sizeof(T) == GetTypeSize(type));
  T def;
  if (IsInteger(type)) {
    def = GetFieldDefaultI<T>(field);
  } else {
    FLATBUFFERS_ASSERT(IsFloat(type));
    def = GetFieldDefaultF<T>(field);
  }
  return table->SetField(field.offset(), val, def);
}

// Raw helper functions used below: set any value in memory as a 64bit int, a
// double or a string.
// These work for all scalar values, but do nothing for other data types.
// To set a string, see SetString below.
void SetAnyValueI(reflection::BaseType type, uint8_t *data, int64_t val);
void SetAnyValueF(reflection::BaseType type, uint8_t *data, double val);
void SetAnyValueS(reflection::BaseType type, uint8_t *data, const char *val);

// Set any table field as a 64bit int, regardless of type what it is.
inline bool SetAnyFieldI(Table *table, const reflection::Field &field,
                         int64_t val) {
  auto field_ptr = table->GetAddressOf(field.offset());
  if (!field_ptr) return val == GetFieldDefaultI<int64_t>(field);
  SetAnyValueI(field.type()->base_type(), field_ptr, val);
  return true;
}

// Set any table field as a double, regardless of what type it is.
inline bool SetAnyFieldF(Table *table, const reflection::Field &field,
                         double val) {
  auto field_ptr = table->GetAddressOf(field.offset());
  if (!field_ptr) return val == GetFieldDefaultF<double>(field);
  SetAnyValueF(field.type()->base_type(), field_ptr, val);
  return true;
}

// Set any table field as a string, regardless of what type it is.
inline bool SetAnyFieldS(Table *table, const reflection::Field &field,
                         const char *val) {
  auto field_ptr = table->GetAddressOf(field.offset());
  if (!field_ptr) return false;
  SetAnyValueS(field.type()->base_type(), field_ptr, val);
  return true;
}

// Set any struct field as a 64bit int, regardless of type what it is.
inline void SetAnyFieldI(Struct *st, const reflection::Field &field,
                         int64_t val) {
  SetAnyValueI(field.type()->base_type(), st->GetAddressOf(field.offset()),
               val);
}

// Set any struct field as a double, regardless of type what it is.
inline void SetAnyFieldF(Struct *st, const reflection::Field &field,
                         double val) {
  SetAnyValueF(field.type()->base_type(), st->GetAddressOf(field.offset()),
               val);
}

// Set any struct field as a string, regardless of type what it is.
inline void SetAnyFieldS(Struct *st, const reflection::Field &field,
                         const char *val) {
  SetAnyValueS(field.type()->base_type(), st->GetAddressOf(field.offset()),
               val);
}

// Set any vector element as a 64bit int, regardless of type what it is.
inline void SetAnyVectorElemI(VectorOfAny *vec, reflection::BaseType elem_type,
                              size_t i, int64_t val) {
  SetAnyValueI(elem_type, vec->Data() + GetTypeSize(elem_type) * i, val);
}

// Set any vector element as a double, regardless of type what it is.
inline void SetAnyVectorElemF(VectorOfAny *vec, reflection::BaseType elem_type,
                              size_t i, double val) {
  SetAnyValueF(elem_type, vec->Data() + GetTypeSize(elem_type) * i, val);
}

// Set any vector element as a string, regardless of type what it is.
inline void SetAnyVectorElemS(VectorOfAny *vec, reflection::BaseType elem_type,
                              size_t i, const char *val) {
  SetAnyValueS(elem_type, vec->Data() + GetTypeSize(elem_type) * i, val);
}

// ------------------------- RESIZING SETTERS -------------------------

// "smart" pointer for use with resizing vectors: turns a pointer inside
// a vector into a relative offset, such that it is not affected by resizes.
template<typename T, typename U> class pointer_inside_vector {
 public:
  pointer_inside_vector(T *ptr, std::vector<U> &vec)
      : offset_(reinterpret_cast<uint8_t *>(ptr) -
                reinterpret_cast<uint8_t *>(flatbuffers::vector_data(vec))),
        vec_(vec) {}

  T *operator*() const {
    return reinterpret_cast<T *>(
        reinterpret_cast<uint8_t *>(flatbuffers::vector_data(vec_)) + offset_);
  }
  T *operator->() const { return operator*(); }
  void operator=(const pointer_inside_vector &piv);

 private:
  size_t offset_;
  std::vector<U> &vec_;
};

// Helper to create the above easily without specifying template args.
template<typename T, typename U>
pointer_inside_vector<T, U> piv(T *ptr, std::vector<U> &vec) {
  return pointer_inside_vector<T, U>(ptr, vec);
}

inline const char *UnionTypeFieldSuffix() { return "_type"; }

// Helper to figure out the actual table type a union refers to.
inline const reflection::Object &GetUnionType(
    const reflection::Schema &schema, const reflection::Object &parent,
    const reflection::Field &unionfield, const Table &table) {
  auto enumdef = schema.enums()->Get(unionfield.type()->index());
  // TODO: this is clumsy and slow, but no other way to find it?
  auto type_field = parent.fields()->LookupByKey(
      (unionfield.name()->str() + UnionTypeFieldSuffix()).c_str());
  FLATBUFFERS_ASSERT(type_field);
  auto union_type = GetFieldI<uint8_t>(table, *type_field);
  auto enumval = enumdef->values()->LookupByKey(union_type);
  return *enumval->object();
}

// Changes the contents of a string inside a FlatBuffer. FlatBuffer must
// live inside a std::vector so we can resize the buffer if needed.
// "str" must live inside "flatbuf" and may be invalidated after this call.
// If your FlatBuffer's root table is not the schema's root table, you should
// pass in your root_table type as well.
void SetString(const reflection::Schema &schema, const std::string &val,
               const String *str, std::vector<uint8_t> *flatbuf,
               const reflection::Object *root_table = nullptr);

// Resizes a flatbuffers::Vector inside a FlatBuffer. FlatBuffer must
// live inside a std::vector so we can resize the buffer if needed.
// "vec" must live inside "flatbuf" and may be invalidated after this call.
// If your FlatBuffer's root table is not the schema's root table, you should
// pass in your root_table type as well.
uint8_t *ResizeAnyVector(const reflection::Schema &schema, uoffset_t newsize,
                         const VectorOfAny *vec, uoffset_t num_elems,
                         uoffset_t elem_size, std::vector<uint8_t> *flatbuf,
                         const reflection::Object *root_table = nullptr);

template<typename T>
void ResizeVector(const reflection::Schema &schema, uoffset_t newsize, T val,
                  const Vector<T> *vec, std::vector<uint8_t> *flatbuf,
                  const reflection::Object *root_table = nullptr) {
  auto delta_elem = static_cast<int>(newsize) - static_cast<int>(vec->size());
  auto newelems = ResizeAnyVector(
      schema, newsize, reinterpret_cast<const VectorOfAny *>(vec), vec->size(),
      static_cast<uoffset_t>(sizeof(T)), flatbuf, root_table);
  // Set new elements to "val".
  for (int i = 0; i < delta_elem; i++) {
    auto loc = newelems + i * sizeof(T);
    auto is_scalar = flatbuffers::is_scalar<T>::value;
    if (is_scalar) {
      WriteScalar(loc, val);
    } else {  // struct
      *reinterpret_cast<T *>(loc) = val;
    }
  }
}

// Adds any new data (in the form of a new FlatBuffer) to an existing
// FlatBuffer. This can be used when any of the above methods are not
// sufficient, in particular for adding new tables and new fields.
// This is potentially slightly less efficient than a FlatBuffer constructed
// in one piece, since the new FlatBuffer doesn't share any vtables with the
// existing one.
// The return value can now be set using Vector::MutateOffset or SetFieldT
// below.
const uint8_t *AddFlatBuffer(std::vector<uint8_t> &flatbuf,
                             const uint8_t *newbuf, size_t newlen);

inline bool SetFieldT(Table *table, const reflection::Field &field,
                      const uint8_t *val) {
  FLATBUFFERS_ASSERT(sizeof(uoffset_t) ==
                     GetTypeSize(field.type()->base_type()));
  return table->SetPointer(field.offset(), val);
}

// ------------------------- COPYING -------------------------

// Generic copying of tables from a FlatBuffer into a FlatBuffer builder.
// Can be used to do any kind of merging/selecting you may want to do out
// of existing buffers. Also useful to reconstruct a whole buffer if the
// above resizing functionality has introduced garbage in a buffer you want
// to remove.
// Note: this does not deal with DAGs correctly. If the table passed forms a
// DAG, the copy will be a tree instead (with duplicates). Strings can be
// shared however, by passing true for use_string_pooling.

Offset<const Table *> CopyTable(FlatBufferBuilder &fbb,
                                const reflection::Schema &schema,
                                const reflection::Object &objectdef,
                                const Table &table,
                                bool use_string_pooling = false);

// Verifies the provided flatbuffer using reflection.
// root should point to the root type for this flatbuffer.
// buf should point to the start of flatbuffer data.
// length specifies the size of the flatbuffer data.
bool Verify(const reflection::Schema &schema, const reflection::Object &root,
            const uint8_t *buf, size_t length);

}  // namespace flatbuffers

#endif  // FLATBUFFERS_REFLECTION_H_
