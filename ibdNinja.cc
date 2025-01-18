/*
 * Copyright (c) [2025] [Zhao Song]
 */


#include "ibdNinja.h"
#include "ibdCollations.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <zlib.h>

#include <rapidjson/error/en.h>
#include <algorithm>


namespace ibd_ninja {

#define ninja_warn(format, ...) \
      fprintf(stderr, "[WARNING] %s:%d - " format "\n", \
              __FILE__, __LINE__, ##__VA_ARGS__)

#define ninja_error(format, ...) \
      fprintf(stderr, "[ERROR] %s:%d - " format "\n", \
              __FILE__, __LINE__, ##__VA_ARGS__)

#define ninja_pt(enable, format, ...) \
  do {                                       \
    if (enable) {                          \
      fprintf(stdout, format, ##__VA_ARGS__); \
    }                                      \
  } while (0)


/* ------Properties------ */
template <typename GV>
bool ReadValue(bool* ap, const GV& gv) {
  if (!gv.IsBool()) {
    return false;
  }
  *ap = gv.GetBool();
  return true;
}

template <typename GV>
bool ReadValue(int32_t* ap, const GV& gv) {
  if (!gv.IsInt()) {
    return false;
  }
  *ap = gv.GetInt();
  return true;
}

template <typename GV>
bool ReadValue(uint32_t* ap, const GV& gv) {
  if (!gv.IsUint()) {
    return false;
  }
  *ap = gv.GetUint();
  return true;
}

template <typename GV>
bool ReadValue(int64_t* ap, const GV& gv) {
  if (!gv.IsInt64()) {
    return false;
  }
  *ap = gv.GetInt64();
  return true;
}

template <typename GV>
bool ReadValue(uint64_t* ap, const GV& gv) {
  if (!gv.IsUint64()) {
    return false;
  }
  *ap = gv.GetUint64();
  return true;
}

template <typename GV>
bool ReadValue(std::string* ap, const GV& gv) {
  if (!gv.IsString()) {
    return false;
  }
  *ap = gv.GetString();
  return true;
}

template <typename T, typename GV>
bool Read(T* ap, const GV& gv, const char* key) {
  if (!gv.HasMember(key)) {
    return false;
  }
  return ReadValue(ap, gv[key]);
}

template <typename ENUM_T, typename GV>
bool ReadEnum(ENUM_T* ap, const GV& gv, const char* key) {
  uint64_t v = 0;
  if (!Read(&v, gv, key)) {
    return false;
  }
  *ap = static_cast<ENUM_T>(v);
  return true;
}

bool Properties::ValidKey(const std::string& key) const {
  bool ret = (keys_.empty() || keys_.find(key) != keys_.end());
  return ret;
}

bool GetValue(const std::string& value_str, std::string* value) {
  *value = value_str;
  return true;
}
bool GetValue(const std::string& value_str, bool* value) {
  if (value_str == "true") {
    *value = true;
    return true;
  }
  if (value_str == "false" || value_str == "0") {
    *value = false;
    return true;
  }
  size_t pos = 0;
  if (value_str[pos] == '+' || value_str[pos] == '-') {
    pos += 1;
  }
  bool is_digit = std::all_of(value_str.begin() + pos, value_str.end(),
                              ::isdigit);
  if (is_digit) {
    *value = true;
  } else {
    return false;
  }
  return true;
}
template <typename ENUM_T>
bool GetValue(const std::string& value_str, ENUM_T* value) {
  uint64_t v = 0;
  GetValue(value_str, &v);
  *value = static_cast<ENUM_T>(v);
  return true;
}
bool GetValue(const std::string& value_str, int32_t* value) {
  *value = strtol(value_str.c_str(), nullptr, 10);
  return true;
}
bool GetValue(const std::string& value_str, uint32_t* value) {
  *value = strtoul(value_str.c_str(), nullptr, 10);
  return true;
}
bool GetValue(const std::string& value_str, int64_t* value) {
  *value = strtoll(value_str.c_str(), nullptr, 10);
  return true;
}
bool GetValue(const std::string& value_str, uint64_t* value) {
  *value = strtoull(value_str.c_str(), nullptr, 10);
  return true;
}

template <typename T>
bool Properties::Get(const std::string& key, T* value) const {
  std::string value_str;
  if (!ValidKey(key)) {
    assert(false);
    return false;
  }
  const auto& iter = kvs_.find(key);
  if (iter == kvs_.end()) {
    return false;
  } else {
    value_str = iter->second;
  }
  GetValue(value_str, value);
  return true;
}

bool Properties::Exists(const std::string& key) const {
  if (!ValidKey(key)) {
    return false;
  }
  const auto& iter = kvs_.find(key);
  if (iter != kvs_.end()) {
    return true;
  } else {
    return false;
  }
}

bool Properties::InsertValues(const std::string& opt_string) {
  assert(kvs_.empty());
  bool found_key = false;
  std::string key = "";
  bool found_value = false;
  std::string value = "";
  size_t last_pos = 0;
  for (size_t pos = 0; pos < opt_string.length(); pos++) {
    if (!found_key && opt_string[pos] != '=') {
      continue;
    } else if (opt_string[pos] == '=') {
      found_key = true;
      if (pos - last_pos > 1) {
        if (last_pos == 0) {
          key = std::string(&opt_string[last_pos], pos - last_pos);
        } else {
          key = std::string(&opt_string[last_pos + 1], pos - last_pos - 1);
        }
      }
      last_pos = pos;
      continue;
    }

    if (!found_value && opt_string[pos] != ';') {
      continue;
    } else if (opt_string[pos] == ';') {
      found_value = true;
      if (pos - last_pos > 1) {
        if (last_pos == 0) {
          value = std::string(&opt_string[last_pos], pos - last_pos);
        } else {
          value = std::string(&opt_string[last_pos + 1], pos - last_pos - 1);
        }
      }
      last_pos = pos;
    }

    assert(found_key && found_value);
    if (key.empty()) {
      std::cerr << "[SDI]Found empty Properties::key" << std::endl;
      return false;
    }

    if (ValidKey(key)) {
      kvs_[key] = value;
      key.clear();
      found_key = false;
      value.clear();
      found_value = false;
    } else {
      std::cerr << "[SDI]Found invalid Properties::key, "
                << key << std::endl;
      return false;
    }
  }
  return true;
}

template <typename PP, typename GV>
bool ReadProperties(PP* pp, const GV& gv, const char* key) {
  std::string opt_string;
  if (Read(&opt_string, gv, key)) {
    return pp->InsertValues(opt_string);
  } else {
    return false;
  }
}

/* ------ Column ------ */
const std::set<std::string> Column::default_valid_option_keys = {
  "column_format",
  "geom_type",
  "interval_count",
  "not_secondary",
  "storage",
  "treat_bit_as_char",
  "is_array",
  "gipk" /* generated implicit primary key column */
};

bool Column::Init(const rapidjson::Value& dd_col_obj) {
  Read(&dd_name_, dd_col_obj, "name");
  ReadEnum(&dd_type_, dd_col_obj, "type");
  Read(&dd_is_nullable_, dd_col_obj, "is_nullable");
  Read(&dd_is_zerofill_, dd_col_obj, "is_zerofill");
  Read(&dd_is_unsigned_, dd_col_obj, "is_unsigned");
  Read(&dd_is_auto_increment_, dd_col_obj, "is_auto_increment");
  Read(&dd_is_virtual_, dd_col_obj, "is_virtual");
  ReadEnum(&dd_hidden_, dd_col_obj, "hidden");
  Read(&dd_ordinal_position_, dd_col_obj, "ordinal_position");
  Read(&dd_char_length_, dd_col_obj, "char_length");
  Read(&dd_numeric_precision_, dd_col_obj, "numeric_precision");
  Read(&dd_numeric_scale_, dd_col_obj, "numeric_scale");
  Read(&dd_numeric_scale_null_, dd_col_obj, "numeric_scale_null");
  Read(&dd_datetime_precision_, dd_col_obj, "datetime_precision");
  Read(&dd_datetime_precision_null_, dd_col_obj, "datetime_precision_null");
  Read(&dd_has_no_default_, dd_col_obj, "has_no_default");
  Read(&dd_default_value_null_, dd_col_obj, "default_value_null");
  Read(&dd_srs_id_null_, dd_col_obj, "srs_id_null");
  if (!dd_srs_id_null_) {
    uint32_t srs_id = 0;
    Read(&srs_id, dd_col_obj, "srs_id");
    dd_srs_id_ = srs_id;
  }
  Read(&dd_default_value_, dd_col_obj, "default_value");
  Read(&dd_default_value_utf8_null_, dd_col_obj, "default_value_utf8_null");
  Read(&dd_default_value_utf8_, dd_col_obj, "default_value_utf8");
  Read(&dd_default_option_, dd_col_obj, "default_option");
  Read(&dd_update_option_, dd_col_obj, "update_option");
  Read(&dd_comment_, dd_col_obj, "comment");
  Read(&dd_generation_expression_, dd_col_obj, "generation_expression");
  Read(&dd_generation_expression_utf8_, dd_col_obj,
      "generation_expression_utf8");
  ReadProperties(&dd_options_, dd_col_obj, "options");
  ReadProperties(&dd_se_private_data_, dd_col_obj, "se_private_data");
  Read(&dd_engine_attribute_, dd_col_obj, "engine_attribute");
  Read(&dd_secondary_engine_attribute_, dd_col_obj,
      "secondary_engine_attribute");
  ReadEnum(&dd_column_key_, dd_col_obj, "column_key");
  Read(&dd_column_type_utf8_, dd_col_obj, "column_type_utf8");
  // TODO(Zhao): dd_elements_
  if (dd_col_obj.HasMember("elements") && dd_col_obj["elements"].IsArray()) {
    dd_elements_size_tmp_ = dd_col_obj["elements"].GetArray().Size();
  }
  Read(&dd_collation_id_, dd_col_obj, "collation_id");
  Read(&dd_is_explicit_collation_, dd_col_obj, "is_explicit_collation");

  return true;
}

Column* Column::CreateColumn(const rapidjson::Value& dd_col_obj) {
  Column* column = new Column();
  bool init_ret = column->Init(dd_col_obj);
  if (!init_ret) {
    delete column;
    column = nullptr;
  }
  return column;
}

Column::enum_field_types Column::DDType2FieldType(
                  enum_column_types type) {
  switch (type) {
    // 31 in total
    case enum_column_types::DECIMAL:
      return enum_field_types::MYSQL_TYPE_DECIMAL;
    case enum_column_types::TINY:
      return enum_field_types::MYSQL_TYPE_TINY;
    case enum_column_types::SHORT:
      return enum_field_types::MYSQL_TYPE_SHORT;
    case enum_column_types::LONG:
      return enum_field_types::MYSQL_TYPE_LONG;
    case enum_column_types::FLOAT:
      return enum_field_types::MYSQL_TYPE_FLOAT;
    case enum_column_types::DOUBLE:
      return enum_field_types::MYSQL_TYPE_DOUBLE;
    case enum_column_types::TYPE_NULL:
      return enum_field_types::MYSQL_TYPE_NULL;
    case enum_column_types::TIMESTAMP:
      return enum_field_types::MYSQL_TYPE_TIMESTAMP;
    case enum_column_types::LONGLONG:
      return enum_field_types::MYSQL_TYPE_LONGLONG;
    case enum_column_types::INT24:
      return enum_field_types::MYSQL_TYPE_INT24;
    case enum_column_types::DATE:
      return enum_field_types::MYSQL_TYPE_DATE;
    case enum_column_types::TIME:
      return enum_field_types::MYSQL_TYPE_TIME;
    case enum_column_types::DATETIME:
      return enum_field_types::MYSQL_TYPE_DATETIME;
    case enum_column_types::YEAR:
      return enum_field_types::MYSQL_TYPE_YEAR;
    case enum_column_types::NEWDATE:
      return enum_field_types::MYSQL_TYPE_NEWDATE;
    case enum_column_types::VARCHAR:
      return enum_field_types::MYSQL_TYPE_VARCHAR;
    case enum_column_types::BIT:
      return enum_field_types::MYSQL_TYPE_BIT;
    case enum_column_types::TIMESTAMP2:
      return enum_field_types::MYSQL_TYPE_TIMESTAMP2;
    case enum_column_types::DATETIME2:
      return enum_field_types::MYSQL_TYPE_DATETIME2;
    case enum_column_types::TIME2:
      return enum_field_types::MYSQL_TYPE_TIME2;
    case enum_column_types::NEWDECIMAL:
      return enum_field_types::MYSQL_TYPE_NEWDECIMAL;
    case enum_column_types::ENUM:
      return enum_field_types::MYSQL_TYPE_ENUM;
    case enum_column_types::SET:
      return enum_field_types::MYSQL_TYPE_SET;
    case enum_column_types::TINY_BLOB:
      return enum_field_types::MYSQL_TYPE_TINY_BLOB;
    case enum_column_types::MEDIUM_BLOB:
      return enum_field_types::MYSQL_TYPE_MEDIUM_BLOB;
    case enum_column_types::LONG_BLOB:
      return enum_field_types::MYSQL_TYPE_LONG_BLOB;
    case enum_column_types::BLOB:
      return enum_field_types::MYSQL_TYPE_BLOB;
    case enum_column_types::VAR_STRING:
      return enum_field_types::MYSQL_TYPE_VAR_STRING;
    case enum_column_types::STRING:
      return enum_field_types::MYSQL_TYPE_STRING;
    case enum_column_types::GEOMETRY:
      return enum_field_types::MYSQL_TYPE_GEOMETRY;
    case enum_column_types::JSON:
      return enum_field_types::MYSQL_TYPE_JSON;
    // TODO(Zhao): support other types
    default:
      assert(false);
  }
}

std::string Column::FieldTypeString() {
  enum_field_types field_type = DDType2FieldType(dd_type_);
  switch (field_type) {
    case MYSQL_TYPE_DECIMAL:
      return "DECIMAL";
    case MYSQL_TYPE_TINY:
      return "TINY";
    case MYSQL_TYPE_SHORT:
      return "SHORT";
    case MYSQL_TYPE_LONG:
      return "LONG";
    case MYSQL_TYPE_FLOAT:
      return "FLOAT";
    case MYSQL_TYPE_DOUBLE:
      return "DOUBLE";
    case MYSQL_TYPE_NULL:
      return "NULL";
    case MYSQL_TYPE_TIMESTAMP:
      return "TIMESTAMP";
    case MYSQL_TYPE_LONGLONG:
      return "LONGLONG";
    case MYSQL_TYPE_INT24:
      return "INT24";
    case MYSQL_TYPE_DATE:
      return "DATE";
    case MYSQL_TYPE_TIME:
      return "TIME";
    case MYSQL_TYPE_DATETIME:
      return "DATETIME";
    case MYSQL_TYPE_YEAR:
      return "YEAR";
    case MYSQL_TYPE_NEWDATE:
      return "NEWDATE";
    case MYSQL_TYPE_VARCHAR:
      return "VARCHAR";
    case MYSQL_TYPE_BIT:
      return "BIT";
    case MYSQL_TYPE_TIMESTAMP2:
      return "TIMESTAMP2";
    case MYSQL_TYPE_DATETIME2:
      return "DATETIME2";
    case MYSQL_TYPE_TIME2:
      return "TIME2";
    case MYSQL_TYPE_TYPED_ARRAY:
      return "TYPED_ARRAY";
    case MYSQL_TYPE_INVALID:
      return "INVALID";
    case MYSQL_TYPE_BOOL:
      return "BOOL";
    case MYSQL_TYPE_JSON:
      return "JSON";
    case MYSQL_TYPE_NEWDECIMAL:
      return "NEWDECIMAL";
    case MYSQL_TYPE_ENUM:
      return "ENUM";
    case MYSQL_TYPE_SET:
      return "SET";
    case MYSQL_TYPE_TINY_BLOB:
      return "TINY_BLOB";
    case MYSQL_TYPE_MEDIUM_BLOB:
      return "MEDIUM_BLOB";
    case MYSQL_TYPE_LONG_BLOB:
      return "LONG_BLOB";
    case MYSQL_TYPE_BLOB:
      return "BLOB";
    case MYSQL_TYPE_VAR_STRING:
      return "VAR_STRING";
    case MYSQL_TYPE_STRING:
      return "STRING";
    case MYSQL_TYPE_GEOMETRY:
      return "GEOMETRY";
    default:
      return "UNKNOWN";
  }
}

std::string Column::SeTypeString() {
  switch (ib_mtype_) {
    case DATA_VARCHAR:
      return "DATA_VARCHAR";
    case DATA_CHAR:
      return "DATA_CHAR";
    case DATA_FIXBINARY:
      return "DATA_FIXBINARY";
    case DATA_BINARY:
      return "DATA_BINARY";
    case DATA_BLOB:
      return "DATA_BLOB";
    case DATA_INT:
      return "DATA_INT";
    case DATA_SYS:
      return "DATA_SYS";
    case DATA_FLOAT:
      return "DATA_FLOAT";
    case DATA_DOUBLE:
      return "DATA_DOUBLE";
    case DATA_DECIMAL:
      return "DATA_DECIMAL";
    case DATA_VARMYSQL:
      return "DATA_VARMYSQL";
    case DATA_MYSQL:
      return "DATA_MYSQL";
    case DATA_GEOMETRY:
      return "DATA_GEOMETRY";
    case DATA_POINT:
      return "DATA_POINT";
    case DATA_VAR_POINT:
      return "DATA_VAR_POINT";
    default:
      return "UNKNOWN";
  }
}

Column::enum_field_types Column::FieldType() const {
  switch (DDType2FieldType(dd_type_)) {
    /*
     * 30 in total
     * Missing MYSQL_TYPE_DATE ?
     */
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      // Field_string -> Field_longstr -> Field_str
      return MYSQL_TYPE_STRING;
    case MYSQL_TYPE_VARCHAR:
      // Field_varstring -> Field_longstr -> Field_str
      return MYSQL_TYPE_VARCHAR;
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
      // Field_blob -> Field_longstr -> Field_str
      return MYSQL_TYPE_BLOB;
    case MYSQL_TYPE_GEOMETRY:
      // Field_geom -> Field_blob -> Field_longstr -> Field_str
      return MYSQL_TYPE_GEOMETRY;
    case MYSQL_TYPE_JSON:
      // Field_json -> Field_blob -> Field_longstr -> Field_str
      return MYSQL_TYPE_JSON;
    case MYSQL_TYPE_ENUM:
      // Field_enum -> Field_str
      return MYSQL_TYPE_STRING;
    case MYSQL_TYPE_SET:
      // Field_set -> Field_enum -> Field_str
      return MYSQL_TYPE_STRING;
    case MYSQL_TYPE_DECIMAL:
      // Field_decimal -> Field_real -> Field_num
      return MYSQL_TYPE_DECIMAL;
    case MYSQL_TYPE_NEWDECIMAL:
      // Field_new_decimal -> Field_num
      return MYSQL_TYPE_NEWDECIMAL;
    case MYSQL_TYPE_FLOAT:
      // Field_float -> Field_real -> Field_num
      return MYSQL_TYPE_FLOAT;
    case MYSQL_TYPE_DOUBLE:
      // Field_double -> Field_real -> Field_num
      return MYSQL_TYPE_DOUBLE;
    case MYSQL_TYPE_TINY:
      // Field_tiny -> Field_num
      return MYSQL_TYPE_TINY;
    case MYSQL_TYPE_SHORT:
      // Field_short -> Field_num
      return MYSQL_TYPE_SHORT;
    case MYSQL_TYPE_INT24:
      // Field_medium -> Field_num
      return MYSQL_TYPE_INT24;
    case MYSQL_TYPE_LONG:
      // Field_long -> Field_num
      return MYSQL_TYPE_LONG;
    case MYSQL_TYPE_LONGLONG:
      // Field_longlong -> Field_num
      return MYSQL_TYPE_LONGLONG;
    case MYSQL_TYPE_TIMESTAMP:
      // Field_timestamp -> Field_temporal_with_date_and_time
      //                 -> Field_temporal_with_date
      //                 -> Field_temporal
      return MYSQL_TYPE_TIMESTAMP;
    case MYSQL_TYPE_TIMESTAMP2:
      // Field_timestampf -> Field_temporal_with_date_and_timef
      //                  -> Field_temporal_with_date_and_time
      //                  -> Field_temporal_with_date
      //                  -> Field_temporal
      // !!!
      return MYSQL_TYPE_TIMESTAMP;
    case MYSQL_TYPE_YEAR:
      // Field_year -> Field_tiny -> Field_num
      return MYSQL_TYPE_YEAR;
    case MYSQL_TYPE_NEWDATE:
      // Field_newdate -> Field_temporal_with_date
      //               -> Field_temporal
      // !!!
      return MYSQL_TYPE_DATE;
    case MYSQL_TYPE_TIME:
      // Field_time -> Field_time_common
      //            -> Field_temporal
      return MYSQL_TYPE_TIME;
    case MYSQL_TYPE_TIME2:
      // Field_timef -> Field_time_common
      //             -> Field_temporal
      // !!!
      return MYSQL_TYPE_TIME;
    case MYSQL_TYPE_DATETIME:
      // Field_datetime -> Field_temporal_with_date_and_time
      //                -> Field_temporal_with_date
      //                -> Field_temporal
      return MYSQL_TYPE_DATETIME;
    case MYSQL_TYPE_DATETIME2:
      // Field_datetimef -> Field_temporal_with_date_and_timef
      //                 -> Field_temporal_with_date_and_time
      //                 -> Field_temporal_with_date
      //                 -> Field_temporal
      // !!!
      return MYSQL_TYPE_DATETIME;
    case MYSQL_TYPE_NULL:
      // Field_null -> Field_str
      return MYSQL_TYPE_NULL;
    case MYSQL_TYPE_BIT:
      // (Field_bit_as_char) -> Field_bit
      return MYSQL_TYPE_BIT;
    // TODO(Zhao): support other types

    // These 2 are not valid user defined in DD
    case MYSQL_TYPE_INVALID:
    case MYSQL_TYPE_BOOL:
    default:
      assert(false);
  }
}

uint32_t Column::FieldType2SeType() const {
  // Check real_type()
  if (dd_type_ == enum_column_types::ENUM ||
      dd_type_ == enum_column_types::SET) {
    return DATA_INT;
  }

  switch (FieldType()) {
    case MYSQL_TYPE_VAR_STRING: /* old <= 4.1 VARCHAR */
    case MYSQL_TYPE_VARCHAR:    /* new >= 5.0.3 true VARCHAR */
      if (IsBinary()) {
        return (DATA_BINARY);
      } else if (dd_collation_id_ == 8 /* my_charset_latin1 */) {
        return (DATA_VARCHAR);
      } else {
        return (DATA_VARMYSQL);
      }
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_STRING:
      if (IsBinary()) {
        return (DATA_FIXBINARY);
      } else if (dd_collation_id_ == 8 /* my_charset_latin1 */) {
        return (DATA_CHAR);
      } else {
        return (DATA_MYSQL);
      }
    case MYSQL_TYPE_NEWDECIMAL:
      return (DATA_FIXBINARY);
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_BOOL:
      return (DATA_INT);
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP2:
      // Check real_type()
      switch (dd_type_) {
        case enum_column_types::TIME:
        case enum_column_types::DATETIME:
        case enum_column_types::TIMESTAMP:
          return (DATA_INT);
        default:
          [[fallthrough]];
        case enum_column_types::TIME2:
        case enum_column_types::DATETIME2:
        case enum_column_types::TIMESTAMP2:
          return (DATA_FIXBINARY);
      }
    case MYSQL_TYPE_FLOAT:
      return (DATA_FLOAT);
    case MYSQL_TYPE_DOUBLE:
      return (DATA_DOUBLE);
    case MYSQL_TYPE_DECIMAL:
      return (DATA_DECIMAL);
    case MYSQL_TYPE_GEOMETRY:
      return (DATA_GEOMETRY);
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_JSON:  // JSON fields are stored as BLOBs
      return (DATA_BLOB);
    case MYSQL_TYPE_NULL:
      break;
    default:
      assert(false);
  }
  return 0;
}

#define DIG_PER_DEC1 9
#define PORTABLE_SIZEOF_CHAR_PTR 8
static const int dig2bytes[DIG_PER_DEC1 + 1] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 4};
uint32_t Column::PackLength() const {
  switch (DDType2FieldType(dd_type_)) {
    /*
     * 30 in total
     */
    case MYSQL_TYPE_VAR_STRING:
      return dd_char_length_;
    case MYSQL_TYPE_STRING:
      return dd_char_length_;
    case MYSQL_TYPE_VARCHAR:
      return VarcharLenBytes() + dd_char_length_;
    case MYSQL_TYPE_BLOB:
      return 2 + PORTABLE_SIZEOF_CHAR_PTR;
    case MYSQL_TYPE_MEDIUM_BLOB:
      return 3 + PORTABLE_SIZEOF_CHAR_PTR;
    case MYSQL_TYPE_TINY_BLOB:
      return 1 + PORTABLE_SIZEOF_CHAR_PTR;
    case MYSQL_TYPE_LONG_BLOB:
      return 4 + PORTABLE_SIZEOF_CHAR_PTR;
    case MYSQL_TYPE_GEOMETRY:
      return 4 + PORTABLE_SIZEOF_CHAR_PTR;
    case MYSQL_TYPE_JSON:
      return 4 + PORTABLE_SIZEOF_CHAR_PTR;
    case MYSQL_TYPE_ENUM:
      return dd_elements_size_tmp_ < 256 ? 1 : 2;
    case MYSQL_TYPE_SET: {
      uint64_t len = (dd_elements_size_tmp_ + 7) / 8;
      return len > 4 ? 8 : len;
      }
      [[fallthrough]];
    case MYSQL_TYPE_DECIMAL:
      return dd_char_length_;
    case MYSQL_TYPE_NEWDECIMAL: {
      int precision = dd_numeric_precision_;
      int scale = dd_numeric_scale_;
      int intg = precision - scale;
      int intg0 = intg / DIG_PER_DEC1;
      int frac0 = scale / DIG_PER_DEC1;
      int intg0x = intg - intg0 * DIG_PER_DEC1;
      int frac0x = scale - frac0 * DIG_PER_DEC1;

      assert(scale >= 0 && precision > 0 &&
             scale <= precision);
      assert(intg0x >= 0);
      assert(intg0x <= DIG_PER_DEC1);
      assert(frac0x >= 0);
      assert(frac0x <= DIG_PER_DEC1);
      return intg0 * sizeof(int32_t) + dig2bytes[intg0x] +
             frac0 * sizeof(int32_t) + dig2bytes[frac0x];
      }
      [[fallthrough]];
    case MYSQL_TYPE_FLOAT:
      return sizeof(float);
    case MYSQL_TYPE_DOUBLE:
      return sizeof(double);
    case MYSQL_TYPE_TINY:
      return 1;
    case MYSQL_TYPE_SHORT:
      return 2;
    case MYSQL_TYPE_INT24:
      return 3;
    case MYSQL_TYPE_LONG:
      return 4;
    case MYSQL_TYPE_LONGLONG:
      return 8;
    case MYSQL_TYPE_TIMESTAMP:
      return dd_char_length_;
    case MYSQL_TYPE_TIMESTAMP2:
      return 4 + (dd_datetime_precision_ + 1) / 2;
    case MYSQL_TYPE_YEAR:
      return 1;
    case MYSQL_TYPE_NEWDATE:
      return 3;
    case MYSQL_TYPE_TIME:
      return 3;
    case MYSQL_TYPE_TIME2:
      return 3 + (dd_datetime_precision_ + 1) / 2;
    case MYSQL_TYPE_DATETIME:
      return 8;
    case MYSQL_TYPE_DATETIME2:
      return 5 + (dd_datetime_precision_ + 1) / 2;
    case MYSQL_TYPE_NULL:
      return 0;
    case MYSQL_TYPE_BIT:
      // treat_bit_as_char should be true
      return (dd_char_length_ + 7) / 8;
    // TODO(Zhao): support other types

    // These 2 are not valid user defined in DD
    case MYSQL_TYPE_INVALID:
    case MYSQL_TYPE_BOOL:
    default:
      assert(false);
  }
}

bool Column::IsBinary() const {
  switch (FieldType()) {
    // For Field_str
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_NULL:
      return (dd_collation_id_ == 63 /* my_charset_bin */);
    default:
      return true;
  }
}

bool Column::IsColumnAdded() const {
  if (dd_se_private_data_.Exists("version_added")) {
    return true;
  } else {
    return false;
  }
}

uint32_t Column::GetVersionAdded() const {
  uint32_t version = UINT8_UNDEFINED;
  if (!IsColumnAdded()) {
    return version;
  }
  dd_se_private_data_.Get("version_added", &version);
  return version;
}

bool Column::IsInstantAdded() const {
  if (ib_version_added_ != UINT8_UNDEFINED &&
      ib_version_added_ > 0) {
    return true;
  }
  return false;
}

bool Column::IsColumnDropped() const {
  if (dd_se_private_data_.Exists("version_dropped")) {
    return true;
  } else {
    return false;
  }
}

uint32_t Column::GetVersionDropped() const {
  uint32_t version = UINT8_UNDEFINED;
  if (!IsColumnDropped()) {
    return version;
  }
  dd_se_private_data_.Get("version_dropped", &version);
  return version;
}

bool Column::IsInstantDropped() const {
  if (ib_version_dropped_ != UINT8_UNDEFINED &&
      ib_version_dropped_ > 0) {
    return true;
  } else {
    return false;
  }
}

uint32_t Column::GetFixedSize() {
  switch (ib_mtype_) {
    case DATA_SYS:
    case DATA_CHAR:
    case DATA_FIXBINARY:
    case DATA_INT:
    case DATA_FLOAT:
    case DATA_DOUBLE:
    case DATA_POINT:
      return ib_col_len_;
    case DATA_MYSQL:
      if (IsBinary()) {
        return ib_col_len_;
      // TODO(Zhao): support redundant row format
      } else {
        auto iter = g_collation_map.find(dd_collation_id_);
        assert(iter != g_collation_map.end());
        if (iter->second.min == iter->second.max) {
          return ib_col_len_;
        }
      }
      [[fallthrough]];
    case DATA_VARCHAR:
    case DATA_BINARY:
    case DATA_DECIMAL:
    case DATA_VARMYSQL:
    case DATA_VAR_POINT:
    case DATA_GEOMETRY:
    case DATA_BLOB:
      return (0);
    default:
      assert(0);
  }
}

bool Column::IsDroppedInOrBefore(uint8_t version) const {
  if (!IsInstantDropped()) {
    return false;
  }

  return (GetVersionDropped() <= version);
}

bool Column::IsAddedAfter(uint8_t version) const {
  if (!IsInstantAdded()) {
    return false;
  }

  return (GetVersionAdded() > version);
}

bool Column::IsBigCol() const {
  return (ib_col_len_ > 255 ||
          (ib_mtype_ == DATA_BLOB || ib_mtype_ == DATA_VAR_POINT ||
           ib_mtype_ == DATA_GEOMETRY));
}

/* ------ IndexColumn ------ */
bool IndexColumn::Init(const rapidjson::Value& dd_index_col_obj,
                              const std::vector<Column*>& columns) {
  Read(&dd_ordinal_position_, dd_index_col_obj, "ordinal_position");
  Read(&dd_length_, dd_index_col_obj, "length");
  ReadEnum(&dd_order_, dd_index_col_obj, "order");
  Read(&dd_hidden_, dd_index_col_obj, "hidden");
  Read(&dd_column_opx_, dd_index_col_obj, "column_opx");

  /*
   * Points to the corresponding Column object.
   * NOTICE:
   * The ordinal_position of a Column starts from 1, while
   * the column_opx of an IndexColumn starts from 0.
   * However, this is not an issue, as the columns array starts from 0.
   */
  column_ = columns[dd_column_opx_];
  column_->set_index_column(this);
  return true;
}

IndexColumn* IndexColumn::CreateIndexColumn(
                        const rapidjson::Value& dd_index_col_obj,
                        const std::vector<Column*>& columns) {
  IndexColumn* element = new IndexColumn(false);
  bool init_ret = element->Init(dd_index_col_obj, columns);
  if (!init_ret) {
    delete element;
    element = nullptr;
  }
  return element;
}

// Used only when creating index columns for a dropped column.
IndexColumn* IndexColumn::CreateIndexDroppedColumn(Column* dropped_col) {
  IndexColumn* index_column = new IndexColumn(true);
  index_column->set_column(dropped_col);
  dropped_col->set_index_column(index_column);

  return index_column;
}
// Used only when creating a FTS_DOC_ID index column.
IndexColumn* IndexColumn::CreateIndexFTSDocIdColumn(Column* doc_id_col) {
  IndexColumn* index_column = new IndexColumn(true);
  index_column->set_column(doc_id_col);
  doc_id_col->set_index_column(index_column);

  return index_column;
}

/* ------ Index ------ */
const std::set<std::string> Index::default_valid_option_keys = {
  "block_size",
  "flags",
  "parser_name",
  "gipk" /* generated implicit primary key */
};

bool Index::Init(const rapidjson::Value& dd_index_obj,
                 const std::vector<Column*>& columns) {
  Read(&dd_name_, dd_index_obj, "name");
  Read(&dd_hidden_, dd_index_obj, "hidden");
  Read(&dd_is_generated_, dd_index_obj, "is_generated");
  Read(&dd_ordinal_position_, dd_index_obj, "ordinal_position");
  Read(&dd_comment_, dd_index_obj, "comment");
  ReadProperties(&dd_options_, dd_index_obj, "options");
  ReadProperties(&dd_se_private_data_, dd_index_obj, "se_private_data");
  ReadEnum(&dd_type_, dd_index_obj, "type");
  ReadEnum(&dd_algorithm_, dd_index_obj, "algorithm");
  Read(&dd_is_algorithm_explicit_, dd_index_obj, "is_algorithm_explicit");
  Read(&dd_is_visible_, dd_index_obj, "is_visible");
  Read(&dd_engine_, dd_index_obj, "engine");
  Read(&dd_engine_attribute_, dd_index_obj, "engine_attribute");
  Read(&dd_secondary_engine_attribute_, dd_index_obj,
      "secondary_engine_attribute");
  if (!dd_index_obj.HasMember("elements") ||
      !dd_index_obj["elements"].IsArray()) {
    std::cerr << "[SDI]Can't find index elements" << std::endl;
    return false;
  }
  const rapidjson::Value& elements = dd_index_obj["elements"].GetArray();
  for (rapidjson::SizeType i = 0; i < elements.Size(); i++) {
    if (!elements[i].IsObject()) {
      std::cerr << "[SDI]Index element isn't an object" << std::endl;
      return false;
    }
    IndexColumn* element = IndexColumn::CreateIndexColumn(elements[i], columns);
    if (element == nullptr) {
      return false;
    }
    dd_elements_.push_back(element);
  }
  Read(&dd_tablespace_ref_, dd_index_obj, "tablespace_ref");
  return true;
}

Index* Index::CreateIndex(const rapidjson::Value& dd_index_obj,
                          const std::vector<Column*>& columns,
                          Table* table) {
  Index* index = new Index(table);
  bool init_ret = index->Init(dd_index_obj, columns);
  if (!init_ret) {
    delete index;
    index = nullptr;
  }
  return index;
}

#define HA_NOSAME 1
#define HA_FULLTEXT (1 << 7)
#define HA_SPATIAL (1 << 10)
bool Index::FillIndex(uint32_t ind) {
  s_user_defined_key_parts_ = 0;
  s_key_length_ = 0;
  s_flags_ = 0;

  for (auto* iter : dd_elements_) {
    if (iter->hidden()) {
      continue;
    }
    s_user_defined_key_parts_++;
    s_key_length_ += iter->length();
  }
  switch (type()) {
    case Index::IT_MULTIPLE:
      s_flags_ = 0;
      break;
    case Index::IT_FULLTEXT:
      s_flags_ = HA_FULLTEXT;
      break;
    case Index::IT_SPATIAL:
      s_flags_ = HA_SPATIAL;
      break;
    case Index::IT_PRIMARY:
    case Index::IT_UNIQUE:
      s_flags_ = HA_NOSAME;
      break;
    default:
      assert(0);
      s_flags_ = 0;
  }
  FillSeIndex(ind);
  return true;
}

#define FTS_DOC_ID_COL_NAME "FTS_DOC_ID"
bool Index::IndexAddCol(Column* col, uint32_t prefix_len) {
  if (col->index_column() != nullptr) {
    ib_fields_.push_back(col->index_column());
  } else if (col->name() == FTS_DOC_ID_COL_NAME) {
    /*
     * The FTS_DOC_ID column is not defined in the SDI's PRIMARY index columns,
     * so we need to create it manually.
     */
    IndexColumn* index_column = IndexColumn::CreateIndexFTSDocIdColumn(col);
    ib_fields_.push_back(index_column);
  } else {
    assert(col->IsInstantDropped());
    IndexColumn* index_column = IndexColumn::CreateIndexDroppedColumn(col);
    ib_fields_.push_back(index_column);
  }
  ib_n_def_++;
  if (ib_type_ & HA_SPATIAL &&
      (col->ib_mtype() == DATA_POINT ||
       col->ib_mtype() == DATA_VAR_POINT) &&
      ib_n_def_ == 1) {
    col->index_column()->set_ib_fixed_len(DATA_MBR_LEN);
  } else {
    col->index_column()->set_ib_fixed_len(col->GetFixedSize());
  }

  if (prefix_len && col->index_column()->ib_fixed_len() > prefix_len) {
    col->index_column()->set_ib_fixed_len(prefix_len);
  }

  if (col->index_column()->ib_fixed_len() > DICT_MAX_FIXED_COL_LEN) {
    col->index_column()->set_ib_fixed_len(0);
  }

  if (col->is_nullable() &&
      !col->IsInstantDropped()) {
    ib_n_nullable_++;
  }
  return true;
}

uint32_t Index::GetNOriginalFields() {
  assert(table_->HasInstantCols());
  uint32_t n_inst_cols_v1 =  table_->GetNInstantAddedColV1();
  uint32_t n_drop_cols = table_->GetNInstantDropCols();
  uint32_t n_add_cols = table_->GetNInstantAddCols();
  uint32_t n_instant_fields =
    ib_n_fields_ + n_drop_cols - n_add_cols - n_inst_cols_v1;

  return n_instant_fields;
}

uint32_t Index::GetNNullableBefore(uint32_t nth) {
  uint32_t nullable = 0;
  for (uint32_t i = 0; i < nth; i++) {
    const IndexColumn* index_col = ib_fields_[i];
    assert(!index_col->column()->IsInstantDropped());
    if (index_col->column()->is_nullable()) {
      nullable++;
    }
  }
  return nullable;
}

uint32_t Index::CalculateNInstantNullable(uint32_t n_fields) {
  if (!table_->HasRowVersions()) {
    return GetNNullableBefore(n_fields);
  }

  uint32_t n_drop_nullable_cols = 0;
  uint32_t new_n_nullable = 0;
  for (uint32_t i = 0; i < ib_n_def_; i++) {
    const IndexColumn* index_col = ib_fields_[i];
    if (index_col->column()->IsInstantAdded()) {
      continue;
    }
    if (index_col->column()->IsInstantDropped()) {
      if (index_col->column()->ib_phy_pos() < n_fields &&
          index_col->column()->is_nullable()) {
        n_drop_nullable_cols++;
      }
      continue;
    }

    if (index_col->column()->ib_phy_pos() < n_fields) {
      if (index_col->column()->is_nullable()) {
        new_n_nullable++;
      }
    }
  }
  new_n_nullable += n_drop_nullable_cols;
  return new_n_nullable;
}

bool Index::HasInstantColsOrRowVersions() {
  if (!IsClustered()) {
    return false;
  }
  return (ib_row_versions_ || ib_instant_cols_);
}

uint32_t Index::GetNullableInVersion(uint8_t version) {
  return ib_nullables_[version];
}

uint16_t Index::GetNullableBeforeInstantAddDrop() {
  if (ib_instant_cols_) {
    return ib_n_instant_nullable_;
  }

  if (ib_row_versions_) {
    return GetNullableInVersion(0);
  }

  return ib_n_nullable_;
}

uint16_t Index::GetNUniqueInTree() {
  if (IsClustered()) {
    return static_cast<uint16_t>(ib_n_uniq_);
  } else {
    return static_cast<uint16_t>(GetNFields());
  }
}
uint16_t Index::GetNUniqueInTreeNonleaf() {
  if (ib_type_ & DICT_SPATIAL) {
    // TODO(zhao): support spatial Index
    assert(0);
    return DICT_INDEX_SPATIAL_NODEPTR_SIZE;
  } else {
    return static_cast<uint16_t>(GetNUniqueInTree());
  }
}

IndexColumn* Index::GetPhysicalField(size_t pos) {
  if (ib_row_versions_) {
    return ib_fields_[ib_fields_array_[pos]];
  }

  return ib_fields_[pos];
}

uint32_t Index::GetNFields() const {
  if (table_->HasRowVersions()) {
    return ib_n_total_fields_;
  }
  return ib_n_fields_;
}

#define UNSUPP_INDEX_MASK 0x7
#define UNSUPP_INDEX_MASK_VIRTUAL 0x1
#define UNSUPP_INDEX_MASK_FTS 0x2
#define UNSUPP_INDEX_MASK_SPATIAL 0x4
void Index::PreCheck() {
  if (dd_type_ == IT_FULLTEXT) {
    unsupported_reason_ |= UNSUPP_INDEX_MASK_FTS;
  }
  if (dd_type_ == IT_SPATIAL) {
    unsupported_reason_ |= UNSUPP_INDEX_MASK_SPATIAL;
  }
  for (auto* iter : dd_elements_) {
    if (iter->hidden()) {
      continue;
    }
    if (iter->column()->is_virtual()) {
      unsupported_reason_ |= UNSUPP_INDEX_MASK_VIRTUAL;
      break;
    }
  }
}
bool Index::IsIndexSupported() {
  return (unsupported_reason_ & UNSUPP_INDEX_MASK) == 0;
}
std::string Index::UnsupportedReason() {
  assert(!IsIndexSupported());
  std::string reason = "";
  if (unsupported_reason_ & UNSUPP_INDEX_MASK_VIRTUAL) {
    reason += "[Index using virtual columns as keys]";
  }
  if (unsupported_reason_ & UNSUPP_INDEX_MASK_FTS) {
    reason += "[Fulltext index]";
  }
  if (unsupported_reason_ & UNSUPP_INDEX_MASK_SPATIAL) {
    reason += "[Spatial index]";
  }
  return reason;
}
bool Index::IsIndexParsingRecSupported() {
  if (!table_->IsTableParsingRecSupported()) {
    return false;
  }
  return IsIndexSupported();
  // Additional checks may be needed here.
}
#define FTS_DOC_ID_INDEX_NAME "FTS_DOC_ID_INDEX"
bool Index::FillSeIndex(uint32_t ind) {
  PreCheck();
  if (!IsIndexSupported()) {
    return true;
  }
  ib_n_fields_ = s_user_defined_key_parts_;
  ib_n_uniq_ = ib_n_fields_;
  if (s_flags_ & HA_SPATIAL) {
    ib_type_ = DICT_SPATIAL;
    assert(ib_n_fields_ == 1);
  } else if (s_flags_ & HA_FULLTEXT) {
    ib_type_ = DICT_FTS;
    ib_n_uniq_ = 0;
  } else if (ind == 0) {
    assert(s_flags_ & HA_NOSAME);
    // dd_hidden_ == true means there no explicit primary index
    assert(ib_n_uniq_ > 0 || dd_hidden_ == true);
    if (dd_hidden_ == false) {
      ib_type_ = DICT_CLUSTERED | DICT_UNIQUE;
    } else {
      /*
       * The implicit primary index type is DICT_CLUSTERED.
       * This is made consistent with InnoDB.
       */
      ib_type_ = DICT_CLUSTERED;
    }
  } else {
    ib_type_ = (s_flags_ & HA_NOSAME) ? DICT_UNIQUE : 0;
  }

  ib_n_def_ = 0;
  ib_n_nullable_ = 0;
  ib_fields_.clear();

  for (auto* iter : dd_elements_) {
    if (iter->hidden()) {
      continue;
    }
    uint32_t prefix_len = 0;
    IndexAddCol(iter->column(), prefix_len);
  }

  /*
   * Special case: "FTS_DOC_ID_INDEX"
   * The columns of FTS_DOC_ID_INDEX defined in the SDI are correct,
   * but the information for the FTS_DOC_ID column is
   * missing (e.g., ib_mtype_).
   * Note that we have created a new FTS_DOC_ID in Table::ib_cols_,
   * so use that instead.
   */
  if (dd_name_ == FTS_DOC_ID_INDEX_NAME) {
    for (auto* iter : dd_elements_) {
      if (iter->column()->name() == FTS_DOC_ID_COL_NAME) {
        for (auto* col : *table()->ib_cols()) {
          if (col->name() == FTS_DOC_ID_COL_NAME) {
            iter->set_column(col);
          }
        }
      }
      /*
       * The user has explicitly defined the FTS_DOC_ID column,
       * so no need to add another.
       */
      if (iter->hidden()) {
        IndexAddCol(iter->column(), 0);
      }
    }
  }

  if (IsClustered()) {
    ib_n_user_defined_cols_ = s_user_defined_key_parts_;
    if (!IsIndexUnique()) {
      ib_n_uniq_ += 1;
    }
    uint32_t n_fields_processed = 0;
    while (n_fields_processed < ib_n_def_) {
      IndexColumn* col = ib_fields_[n_fields_processed];
      if (!table()->HasRowVersions()) {
        col->column()->set_ib_phy_pos(n_fields_processed);
      } else {
        assert(col->column()->ib_phy_pos() != UINT32_UNDEFINED);
      }
      n_fields_processed++;
    }
    bool found_db_row_id = false;
    bool found_db_trx_id = false;
    bool found_db_roll_ptr = false;
    for (auto* col : *(table()->ib_cols())) {
      if (!IsIndexUnique() && col->name() == "DB_ROW_ID") {
        found_db_row_id = true;
        if (!table()->HasRowVersions()) {
          col->set_ib_phy_pos(n_fields_processed);
        } else {
          assert(col->ib_phy_pos() != UINT32_UNDEFINED);
        }
        IndexAddCol(col, 0);
        n_fields_processed++;
        ib_n_fields_++;
      }
      if (col->name() == "DB_TRX_ID") {
        found_db_trx_id = true;
        if (!table()->HasRowVersions()) {
          col->set_ib_phy_pos(n_fields_processed);
        } else {
          assert(col->ib_phy_pos() != UINT32_UNDEFINED);
        }
        IndexAddCol(col, 0);
        n_fields_processed++;
        ib_n_fields_++;
      }
      if (col->name() == "DB_ROLL_PTR") {
        found_db_roll_ptr = true;
        if (!table()->HasRowVersions()) {
          col->set_ib_phy_pos(n_fields_processed);
        } else {
          assert(col->ib_phy_pos() != UINT32_UNDEFINED);
        }
        IndexAddCol(col, 0);
        n_fields_processed++;
        ib_n_fields_++;
      }
    }
    assert((IsIndexUnique() || found_db_row_id) &&
        found_db_trx_id && found_db_roll_ptr);

    std::vector<bool> indexed(table()->GetTotalCols(), false);
    for (auto* iter : ib_fields_) {
      indexed[iter->column()->ib_ind()] = true;
    }
    for (uint32_t i = 0; i < table()->ib_n_cols() - DATA_N_SYS_COLS; i++) {
      Column* col = table()->ib_cols()->at(i);
      assert(col->ib_mtype() != DATA_SYS);
      if (indexed[col->ib_ind()]) {
        continue;
      }
      if (!table()->HasRowVersions()) {
        col->set_ib_phy_pos(n_fields_processed);
      } else {
        assert(col->ib_phy_pos() != UINT32_UNDEFINED);
      }

      IndexAddCol(col, 0);
      n_fields_processed++;
      ib_n_fields_++;
    }
    for (uint32_t i = table()->ib_n_cols(); i < table()->GetTotalCols(); i++) {
      Column* col = table()->ib_cols()->at(i);
      assert(col->ib_mtype() != DATA_SYS);
      IndexAddCol(col, 0);
      n_fields_processed++;
      // Do not add ib_n_fields_ here, as we are adding dropped columns.
    }
    if (!table()->ib_is_system_table()) {
      ib_fields_array_.clear();
      memset(ib_nullables_, 0,
          (MAX_ROW_VERSION + 1) * sizeof(ib_nullables_[0]));
      if (table()->HasRowVersions()) {
        ib_fields_array_.resize(ib_n_def_);
        for (uint32_t i = 0; i < ib_n_def_; i++) {
          IndexColumn *index_col = ib_fields_[i];
          assert(index_col != nullptr && index_col->column() != nullptr);

          size_t pos = index_col->column()->ib_phy_pos();

          ib_fields_array_[pos] = i;
        }

        uint32_t current_row_version = table()->ib_current_row_version();
        auto update_nullable = [&](size_t start_version, bool is_increment) {
          for (size_t i = start_version; i <= current_row_version; i++) {
            assert(is_increment || ib_nullables_[i] > 0);

            if (is_increment) {
              ++ib_nullables_[i];
            } else {
              --ib_nullables_[i];
            }
          }
        };
        for (uint32_t i = 0; i < ib_n_def_; i++) {
          IndexColumn *index_col = ib_fields_[i];

          if (index_col->column()->name() == "DB_ROW_ID" ||
               index_col->column()->name() == "DB_TRX_ID" ||
               index_col->column()->name() == "DB_ROLL_PTR") {
            continue;
          }

          if (!index_col->column()->is_nullable()) {
            continue;
          }

          size_t start_from = 0;
          if (index_col->column()->IsInstantAdded()) {
            start_from = index_col->column()->GetVersionAdded();
          }
          update_nullable(start_from, true);

          if (index_col->column()->IsInstantDropped()) {
            update_nullable(index_col->column()->GetVersionDropped(), false);
          }
        }
      }
    }
    assert(table_->clust_index() == nullptr);
    table_->set_clust_index(this);
  } else {
    ib_n_user_defined_cols_ = s_user_defined_key_parts_;
    std::vector<bool> indexed(table()->GetTotalCols(), false);
    for (auto* iter : ib_fields_) {
      if (iter->column()->is_virtual()) {
        continue;
      }
      indexed[iter->column()->ib_ind()] = true;
    }
    assert(table_->clust_index() != nullptr);
    for (uint32_t i = 0; i < table_->clust_index()->ib_n_uniq(); i++) {
      IndexColumn* clust_index_col = table_->clust_index()->ib_fields()->at(i);
      if (!indexed[clust_index_col->column()->ib_ind()]) {
        uint32_t prefix_len = 0;
        IndexAddCol(clust_index_col->column(), prefix_len);
      } else {
        // TODO(Zhao): Support spatial index
      }
    }

    if (IsIndexUnique()) {
      ib_n_uniq_ = ib_n_fields_;
    } else {
      ib_n_uniq_ = ib_n_def_;
    }
    ib_n_fields_ = ib_n_def_;
  }
  dd_se_private_data_.Get("id", &ib_id_);
  dd_se_private_data_.Get("root", &ib_page_);

  ib_n_fields_ = ib_n_def_;

  if (IsClustered() && table_->HasRowVersions()) {
    ib_n_fields_ = ib_n_def_ - table_->GetNInstantDropCols();
  }

  ib_n_total_fields_ = ib_n_def_;
  ib_row_versions_ = false;
  ib_instant_cols_ = false;
  ib_n_instant_nullable_ = ib_n_nullable_;
  if (IsClustered()) {
    ib_row_versions_ = table_->HasRowVersions();
    if (table_->HasInstantCols()) {
      ib_instant_cols_ = true;
      uint32_t n_instant_fields = GetNOriginalFields();
      uint32_t new_n_nullable = CalculateNInstantNullable(n_instant_fields);
      ib_n_instant_nullable_ = new_n_nullable;
    }
  }
  return true;
}

/* ------ Table ------ */
const std::set<std::string> Table::default_valid_option_keys = {
  "avg_row_length",
  "checksum",
  "compress",
  "connection_string",
  "delay_key_write",
  "encrypt_type",
  "explicit_tablespace",
  "key_block_size",
  "keys_disabled",
  "max_rows",
  "min_rows",
  "pack_keys",
  "pack_record",
  "plugin_version",
  "row_type",
  "secondary_engine",
  "secondary_load",
  "server_i_s_table",
  "server_p_s_table",
  "stats_auto_recalc",
  "stats_persistent",
  "stats_sample_pages",
  "storage",
  "tablespace",
  "timestamp",
  "view_valid",
  "gipk"
};

bool Table::Init(const rapidjson::Value& dd_obj) {
  // Table
  Read(&dd_name_, dd_obj, "name");
  Read(&dd_mysql_version_id_, dd_obj, "mysql_version_id");
  Read(&dd_created_, dd_obj, "created");
  Read(&dd_last_altered_, dd_obj, "last_altered");
  ReadEnum(&dd_hidden_, dd_obj, "hidden");
  ReadProperties(&dd_options_, dd_obj, "options");

  // Columns
  if (!dd_obj.HasMember("columns") || !dd_obj["columns"].IsArray()) {
    std::cerr << "[SDI]Can't find columns" << std::endl;
    return false;
  }
  const rapidjson::Value& columns = dd_obj["columns"].GetArray();
  for (rapidjson::SizeType i = 0; i < columns.Size(); i++) {
    if (!columns[i].IsObject()) {
      std::cerr << "[SDI]Column isn't an object" << std::endl;
      return false;
    }
    Column* column = Column::CreateColumn(columns[i]);
    if (column == nullptr) {
      return false;
    }
    columns_.push_back(column);
  }

  // Table
  Read(&dd_schema_ref_, dd_obj, "schema_ref");
  Read(&dd_se_private_id_, dd_obj, "se_private_id");
  Read(&dd_engine_, dd_obj, "engine");
  Read(&dd_last_checked_for_upgrade_version_id_, dd_obj,
       "last_checked_for_upgrade_version_id");
  Read(&dd_comment_, dd_obj, "comment");
  ReadProperties(&dd_se_private_data_, dd_obj, "se_private_data");
  Read(&dd_engine_attribute_, dd_obj, "engine_attribute");
  Read(&dd_secondary_engine_attribute_, dd_obj, "secondary_engine_attribute");
  ReadEnum(&dd_row_format_, dd_obj, "row_format");
  ReadEnum(&dd_partition_type_, dd_obj, "partition_type");
  Read(&dd_partition_expression_, dd_obj, "partition_expression");
  Read(&dd_partition_expression_utf8_, dd_obj, "partition_expression_utf8");
  ReadEnum(&dd_default_partitioning_, dd_obj, "default_partitioning");
  ReadEnum(&dd_subpartition_type_, dd_obj, "subpartition_type");
  Read(&dd_subpartition_expression_, dd_obj, "subpartition_expression");
  Read(&dd_subpartition_expression_utf8_, dd_obj,
       "subpartition_expression_utf8");
  ReadEnum(&dd_default_subpartitioning_, dd_obj, "default_subpartitioning");
  ReadEnum(&dd_collation_id_, dd_obj, "collation_id");

  // Indexes
  if (!dd_obj.HasMember("indexes") || !dd_obj["indexes"].IsArray()) {
    std::cerr << "Can't find indexes" << std::endl;
    return false;
  }
  const rapidjson::Value& indexes = dd_obj["indexes"].GetArray();
  for (rapidjson::SizeType i = 0; i < indexes.Size(); i++) {
    if (!indexes[i].IsObject()) {
      std::cerr << "Index isn't an object" << std::endl;
      return false;
    }
    Index* index = Index::CreateIndex(indexes[i], columns_, this);
    if (index == nullptr) {
      return false;
    }
    indexes_.push_back(index);
  }

  /* TABLE_SHARE */
  s_field_.clear();
  for (auto* iter : columns_) {
    if (iter->IsSeHidden()) {
      continue;
    }
    if (iter->is_nullable()) {
      s_null_fields_++;
    }
    s_fields_++;
    s_field_.push_back(iter);
  }

  /* SE */
  if (!InitSeTable()) {
    return false;
  }
  return true;
}

Column* Table::FindColumn(const std::string& col_name) {
  for (const auto& iter : columns_) {
    if (iter->name() == col_name) {
      return iter;
    }
  }
  return nullptr;
}

bool Table::ContainFulltext() {
  for (const auto& iter : indexes_) {
    if (iter->type() == Index::enum_index_type::IT_FULLTEXT) {
      return true;
    }
  }
  return false;
}

#define UNSUPP_TABLE_MASK 0x1F
#define UNSUPP_TABLE_MASK_PARTITION 0x1
#define UNSUPP_TABLE_MASK_ENCRYPT 0x2
#define UNSUPP_TABLE_MASK_FTS_AUX_INDEX 0x4
#define UNSUPP_TABLE_MASK_FTS_COM_INDEX 0x8
#define UNSUPP_TABLE_MASK_VERSION 0x10
void Table::PreCheck() {
  // TODO(Zhao): Support more MySQL versions
  if (dd_mysql_version_id_ < 80016 || dd_mysql_version_id_ > 80040) {
    unsupported_reason_ |= UNSUPP_TABLE_MASK_VERSION;
  }

  if (dd_partition_type_ != enum_partition_type::PT_NONE) {
    unsupported_reason_ |= UNSUPP_TABLE_MASK_PARTITION;
  }

  if (dd_options_.Exists("encrypt_type")) {
    std::string encrypted;
    dd_options_.Get("encrypt_type", &encrypted);
    if (encrypted != "" && encrypted != "N" && encrypted != "n") {
      unsupported_reason_ |= UNSUPP_TABLE_MASK_ENCRYPT;
    }
  }

  for (const auto& index : indexes_) {
    if (index->name() == "FTS_INDEX_TABLE_IND" &&
        dd_hidden_ == enum_hidden_type::HT_HIDDEN_SE) {
      unsupported_reason_ |= UNSUPP_TABLE_MASK_FTS_AUX_INDEX;
      continue;
    }
    if (index->name() == "FTS_COMMON_TABLE_IND" &&
        dd_hidden_ == enum_hidden_type::HT_HIDDEN_SE) {
      unsupported_reason_ |= UNSUPP_TABLE_MASK_FTS_COM_INDEX;
      continue;
    }
  }
}

bool Table::IsTableSupported() {
  return (unsupported_reason_ & UNSUPP_TABLE_MASK) == 0;
}

std::string Table::UnsupportedReason() {
  assert(!IsTableSupported());
  std::string reason = "";
  if (unsupported_reason_ & UNSUPP_TABLE_MASK_PARTITION) {
    reason += "[Partition table]";
  }
  if (unsupported_reason_ & UNSUPP_TABLE_MASK_ENCRYPT) {
    reason += "[Encrpted table]";
  }
  if (unsupported_reason_ & UNSUPP_TABLE_MASK_FTS_AUX_INDEX) {
    reason += "[FTS Auxiliary index table]";
  }
  if (unsupported_reason_ & UNSUPP_TABLE_MASK_FTS_COM_INDEX) {
    reason += "[FTS Common index table]";
  }
  if (unsupported_reason_ & UNSUPP_TABLE_MASK_VERSION) {
    reason += ("[Table was created in unsupported version " +
                std::to_string(dd_mysql_version_id_) +
                ", expected in [80016, 80040] ]");
  }
  return reason;
}

bool Table::InitSeTable() {
  PreCheck();
  if (!IsTableSupported()) {
    return true;
  }
  std::string norm_name = dd_schema_ref_ + "/" + dd_name_;

  if (dd_schema_ref_ == "mysql" && dd_schema_ref_ == "information_schema" &&
      dd_schema_ref_ == "performance_schema") {
    ib_is_system_table_ = true;
  } else {
    ib_is_system_table_ = false;
  }

  [[maybe_unused]] bool is_encrypted = false;
  if (dd_options_.Exists("encrypt_type")) {
    std::string encrypted;
    dd_options_.Get("encrypt_type", &encrypted);
    if (encrypted != "" && encrypted != "N" && encrypted != "n") {
      is_encrypted = true;
    }
  }

  bool has_doc_id = false;
  const Column* col = FindColumn(FTS_DOC_ID_COL_NAME);
  if (col != 0 && col->type() == Column::enum_column_types::LONGLONG &&
      !col->is_nullable()) {
    has_doc_id = true;
  }
  bool add_doc_id = false;
  if (has_doc_id && col->IsSeHidden()) {
    add_doc_id = true;
  }

  // TODO(Zhao): Support partition table

  uint32_t n_cols = s_fields_ + (add_doc_id ? 1 : 0);
  uint32_t n_v_cols = 0;
  uint32_t n_m_v_cols = 0;
  for (const auto& iter : columns_) {
    if (iter->IsSeHidden()) {
      continue;
    }
    if (iter->is_virtual()) {
      n_v_cols++;
      if (iter->options().Exists("is_array")) {
        bool is_array = false;
        iter->options().Get("is_array", &is_array);
        if (is_array) {
          n_m_v_cols++;
        }
      }
    }
  }

  uint32_t current_row_version = 0;
  uint32_t n_current_cols = 0;
  uint32_t n_initial_cols = 0;
  uint32_t n_total_cols = 0;

  uint32_t n_dropped_cols = 0;
  uint32_t n_added_cols = 0;
  uint32_t n_added_and_dropped_cols = 0;
  bool has_row_version = false;
  for (const auto& iter : columns_) {
    if (iter->IsSystemColumn() || iter->is_virtual()) {
      continue;
    }

    if (!has_row_version) {
      if (iter->se_private_data().Exists("physical_pos")) {
        has_row_version = true;
      }
    }

    if (iter->IsColumnDropped()) {
      n_dropped_cols++;
      if (iter->IsColumnAdded()) {
        n_added_and_dropped_cols++;
      }
      uint32_t v_dropped = iter->GetVersionDropped();
      current_row_version = std::max(current_row_version, v_dropped);
      continue;
    }

    if (iter->IsColumnAdded()) {
      n_added_cols++;
      uint32_t v_added = iter->GetVersionAdded();
      current_row_version = std::max(current_row_version, v_added);
    }
    n_current_cols++;
  }
  n_initial_cols = (n_current_cols - n_added_cols) +
                   (n_dropped_cols - n_added_and_dropped_cols);
  n_total_cols = n_current_cols + n_dropped_cols;

  ib_n_t_cols_ = n_cols + DATA_N_SYS_COLS;
  ib_n_v_cols_ = n_v_cols;
  ib_n_m_v_cols_ = n_m_v_cols;
  ib_n_cols_ = ib_n_t_cols_ - ib_n_v_cols_;
  ib_n_instant_cols_ = ib_n_cols_;
  ib_initial_col_count_ = n_initial_cols;
  ib_current_col_count_ = n_current_cols;
  ib_total_col_count_ = n_total_cols;
  ib_current_row_version_ = current_row_version;
  ib_m_upgraded_instant_ = false;

  ib_id_ = dd_se_private_id_;

  if (dd_se_private_data_.Exists("instant_col")) {
    uint32_t n_inst_cols = 0;
    if (dd_partition_type_ != enum_partition_type::PT_NONE) {
      // TODO(Zhao): Support partition table
    } else {
      dd_se_private_data_.Get("instant_col", &n_inst_cols);
      ib_n_instant_cols_ = n_inst_cols + DATA_N_SYS_COLS;
      ib_m_upgraded_instant_ = true;
    }
  }

  ib_cols_.clear();
  ib_n_def_ = 0;
  ib_n_v_def_ = 0;
  ib_n_t_def_ = 0;

  for (auto* iter : s_field_) {
    iter->set_ib_mtype(iter->FieldType2SeType());
    ib_n_t_def_++;
    if (!iter->is_virtual()) {
      iter->set_ib_ind(ib_n_def_);
      ib_n_def_++;
      uint32_t v_added = iter->GetVersionAdded();
      uint32_t phy_pos = UINT32_UNDEFINED;
      bool is_hidden_by_system =
        (iter->hidden() == Column::enum_hidden_type::HT_HIDDEN_SE ||
         iter->hidden() == Column::enum_hidden_type::HT_HIDDEN_SQL);
      if (has_row_version) {
        if (iter->se_private_data().Exists("physical_pos")) {
          iter->se_private_data().Get("physical_pos", &phy_pos);
          assert(phy_pos != UINT32_UNDEFINED);
        }
      }
      iter->set_ib_is_visible(!is_hidden_by_system);
      iter->set_ib_version_added(v_added);
      iter->set_ib_version_dropped(UINT8_UNDEFINED);
      iter->set_ib_phy_pos(phy_pos);
      if (iter->FieldType() == Column::enum_field_types::MYSQL_TYPE_VARCHAR) {
        // The col_len of VARCHAR in InnoDB does not include the length header.
        uint32_t col_len = iter->PackLength() - iter->VarcharLenBytes();
        iter->set_ib_col_len(col_len);
      } else {
        iter->set_ib_col_len(iter->PackLength());
      }

      ib_cols_.push_back(iter);
    } else {
      ib_n_v_def_++;
    }
  }

  if (add_doc_id) {
    Column* doc_id_col = new Column(FTS_DOC_ID_COL_NAME, ib_n_def_, true);
    ib_n_t_def_++;
    ib_n_def_++;
    doc_id_col->set_type(Column::enum_column_types::LONGLONG);
    doc_id_col->set_ib_mtype(DATA_INT);
    doc_id_col->set_ib_col_len(sizeof(uint64_t));
    ib_cols_.push_back(doc_id_col);
  }

  Column* row_id_col = FindColumn("DB_ROW_ID");
  if (row_id_col != nullptr) {
    // DB_ROW_ID is already in the columns_
    ib_n_t_def_++;
    row_id_col->set_ib_ind(ib_n_def_);
    ib_n_def_++;
    row_id_col->set_ib_mtype(DATA_SYS);
    row_id_col->set_ib_is_visible(false);
    row_id_col->set_ib_version_added(0);
    row_id_col->set_ib_version_dropped(0);
    uint32_t phy_pos = UINT32_UNDEFINED;
    if (has_row_version) {
      if (row_id_col->se_private_data().Exists("physical_pos")) {
        row_id_col->se_private_data().Get("physical_pos", &phy_pos);
      }
    }
    row_id_col->set_ib_phy_pos(phy_pos);
    row_id_col->set_ib_col_len(DATA_ROW_ID_LEN);
    ib_cols_.push_back(row_id_col);
  } else {
    row_id_col_ = new Column("DB_ROW_ID", ib_n_def_);
    ib_n_t_def_++;
    ib_n_def_++;
    row_id_col_->set_ib_col_len(DATA_ROW_ID_LEN);
    row_id_col_->set_type(Column::enum_column_types::INT24);
    ib_cols_.push_back(row_id_col_);
  }

  Column* trx_id_col = FindColumn("DB_TRX_ID");
  assert(trx_id_col != nullptr);
  if (trx_id_col != nullptr) {
    ib_n_t_def_++;
    trx_id_col->set_ib_ind(ib_n_def_);
    ib_n_def_++;
    trx_id_col->set_ib_mtype(DATA_SYS);
    trx_id_col->set_ib_is_visible(false);
    trx_id_col->set_ib_version_added(0);
    trx_id_col->set_ib_version_dropped(0);
    uint32_t phy_pos = UINT32_UNDEFINED;
    if (has_row_version) {
      if (trx_id_col->se_private_data().Exists("physical_pos")) {
        trx_id_col->se_private_data().Get("physical_pos", &phy_pos);
      }
    }
    trx_id_col->set_ib_phy_pos(phy_pos);
    trx_id_col->set_ib_col_len(DATA_TRX_ID_LEN);
    ib_cols_.push_back(trx_id_col);
  }

  Column* roll_ptr_col = FindColumn("DB_ROLL_PTR");
  assert(roll_ptr_col != nullptr);
  if (roll_ptr_col != nullptr) {
    ib_n_t_def_++;
    roll_ptr_col->set_ib_ind(ib_n_def_);
    ib_n_def_++;
    roll_ptr_col->set_ib_mtype(DATA_SYS);
    roll_ptr_col->set_ib_is_visible(false);
    roll_ptr_col->set_ib_version_added(0);
    roll_ptr_col->set_ib_version_dropped(0);
    uint32_t phy_pos = UINT32_UNDEFINED;
    if (has_row_version) {
      if (roll_ptr_col->se_private_data().Exists("physical_pos")) {
        roll_ptr_col->se_private_data().Get("physical_pos", &phy_pos);
      }
    }
    roll_ptr_col->set_ib_phy_pos(phy_pos);
    roll_ptr_col->set_ib_col_len(DATA_ROLL_PTR_LEN);
    ib_cols_.push_back(roll_ptr_col);
  }

  if (HasInstantDropCols()) {
    for (auto* iter : columns_) {
      if (iter->IsSystemColumn()) {
        continue;
      }
      if (iter->IsColumnDropped()) {
        iter->set_ib_mtype(iter->FieldType2SeType());
        iter->set_ib_ind(ib_n_def_);
        ib_n_def_++;
        ib_n_t_def_++;
        uint32_t v_added = iter->GetVersionAdded();
        uint32_t v_dropped = iter->GetVersionDropped();
        uint32_t phy_pos = UINT32_UNDEFINED;
        assert(iter->se_private_data().Exists("physical_pos"));
        iter->se_private_data().Get("physical_pos", &phy_pos);
        assert(phy_pos != UINT32_UNDEFINED);
        iter->set_ib_is_visible(false);
        iter->set_ib_version_added(v_added);
        iter->set_ib_version_dropped(v_dropped);
        iter->set_ib_phy_pos(phy_pos);
        if (iter->FieldType() == Column::enum_field_types::MYSQL_TYPE_VARCHAR) {
          // The col_len of VARCHAR in InnoDB does not include the length header.
          uint32_t col_len = iter->PackLength() - iter->VarcharLenBytes();
          iter->set_ib_col_len(col_len);
        } else {
          iter->set_ib_col_len(iter->PackLength());
        }
        ib_cols_.push_back(iter);
      }
    }
  }

  if (HasInstantCols() || HasRowVersions()) {
    // TODO(Zhao): Support partition table
    for (auto* iter : columns_) {
      iter->set_ib_instant_default(false);
      if (iter->is_virtual() || iter->IsSystemColumn()) {
        continue;
      }
      if (iter->IsColumnDropped()) {
        continue;
      }
      if (!iter->se_private_data().Exists("default_null") &&
          !iter->se_private_data().Exists("default")) {
        // This is not INSTANT ADD column
        continue;
      }

      if (iter->se_private_data().Exists("default_null")) {
        iter->set_ib_instant_default(false);
      } else if (iter->se_private_data().Exists("default")) {
        iter->set_ib_instant_default(true);
        // TODO(Zhao): parse the default value
      }
    }
  }

  assert(!indexes_.empty());
  uint32_t ind = 0;
  for (auto* iter : indexes_) {
    iter->FillIndex(ind);
    ind++;
  }

  return true;
}

Table* Table::CreateTable(const rapidjson::Value& dd_obj,
                          unsigned char* sdi_data) {
  Table* table = new Table(sdi_data);
  bool init_ret = table->Init(dd_obj);
  if (!init_ret) {
    delete table;
    table = nullptr;
  }
  return table;
}

bool Table::IsTableParsingRecSupported() {
  if (!IsTableSupported()) {
    return false;
  }
  if (dd_row_format_ != RF_DYNAMIC &&
      dd_row_format_ != RF_COMPACT) {
    ninja_error("Parsing of record with row format %s is not yet supported",
                 RowFormatString().c_str());
    return false;
  }
  return true;
}

/* ------ Record ------ */
uint32_t Record::GetBitsFrom1B(uint32_t offs, uint32_t mask, uint32_t shift) {
  return ((ReadFrom1B(rec_ - offs) & mask) >> shift);
}
uint32_t Record::GetBitsFrom2B(uint32_t offs, uint32_t mask, uint32_t shift) {
  return ((ReadFrom2B(rec_ - offs) & mask) >> shift);
}

uint32_t Record::GetStatus() {
  uint32_t ret = 0;
  ret = GetBitsFrom1B(REC_NEW_STATUS, REC_NEW_STATUS_MASK,
                      REC_NEW_STATUS_SHIFT);
  assert((ret & ~REC_NEW_STATUS_MASK) == 0);
  return ret;
}

uint32_t* Record::GetColumnOffsets() {
  uint32_t n = 0;
  if (index_->table()->IsCompact()) {
    switch (GetStatus()) {
      case REC_STATUS_ORDINARY:
       n = index_->GetNFields();
       break;
      case REC_STATUS_NODE_PTR:
       n = index_->GetNUniqueInTreeNonleaf() + 1;
       break;
      case REC_STATUS_INFIMUM:
      case REC_STATUS_SUPREMUM:
        n = 1;
        break;
      default:
        assert(0);
    }
  } else {
    // TODO(Zhao): Support redundant row format
  }
  uint32_t size = n + (1 + REC_OFFS_HEADER_SIZE);
  assert(offsets_ == nullptr);
  offsets_ = new uint32_t[size];
  SetNAlloc(size);
  SetNFields(n);
  InitColumnOffsets();

  return offsets_;
}

static uint32_t* RecOffsBase(uint32_t* offsets) {
  return offsets + REC_OFFS_HEADER_SIZE;
}

static uint32_t RecOffsNFields(uint32_t* offsets) {
  uint32_t n_fields = offsets[1];
  assert(n_fields > 0);
  return n_fields;
}

void Record::InitColumnOffsetsCompact() {
  uint32_t n_node_ptr_field = ULINT_UNDEFINED;
  switch (GetStatus()) {
    case REC_STATUS_INFIMUM:
    case REC_STATUS_SUPREMUM:
      RecOffsBase(offsets_)[0] = REC_N_NEW_EXTRA_BYTES | REC_OFFS_COMPACT;
      RecOffsBase(offsets_)[1] = 8;
      return;
    case REC_STATUS_NODE_PTR:
      n_node_ptr_field = index_->GetNUniqueInTreeNonleaf();
      break;
    case REC_STATUS_ORDINARY:
      InitColumnOffsetsCompactLeaf();
      return;
  }

  // non-leaf page record
  assert(!IsVersionedCompact());
  const unsigned char* nulls = rec_ - (REC_N_NEW_EXTRA_BYTES + 1);
  const size_t nullable_cols = index_->GetNullableBeforeInstantAddDrop();

  const unsigned char* lens = nulls - UT_BITS_IN_BYTES(nullable_cols);
  uint32_t offs = 0;
  uint32_t null_mask = 1;

  uint32_t i = 0;
  IndexColumn* index_col = nullptr;
  Column* col = nullptr;
  do {
    uint32_t len;
    if (i == n_node_ptr_field) {
      len = offs += REC_NODE_PTR_SIZE;
      goto resolved;
    }

    index_col = index_->ib_fields()->at(i);
    col = index_col->column();
    if (col->is_nullable()) {
      if (!(unsigned char)null_mask) {
        nulls--;
        null_mask = 1;
      }

      if (*nulls & null_mask) {
        null_mask <<= 1;
        len = offs | REC_OFFS_SQL_NULL;
        goto resolved;
      }
      null_mask <<= 1;
    }

    if (!index_col->ib_fixed_len()) {
      /* DATA_POINT should always be a fixed
      length column. */
      assert(col->ib_mtype() != DATA_POINT);
      /* Variable-length field: read the length */
      len = *lens--;
      if (col->IsBigCol()) {
        if (len & 0x80) {
          len <<= 8;
          len |= *lens--;
          assert(!(len & 0x4000));
          offs += len & 0x3fff;
          len = offs;

          goto resolved;
        }
      }

      len = offs += len;
    } else {
      len = offs += index_col->ib_fixed_len();
    }
  resolved:
    RecOffsBase(offsets_)[i + 1] = len;
  } while (++i < RecOffsNFields(offsets_));

  *RecOffsBase(offsets_) = (rec_ - (lens + 1)) | REC_OFFS_COMPACT;
}

void Record::InitColumnOffsets() {
  if (index_->table()->IsCompact()) {
    InitColumnOffsetsCompact();
  } else {
    // TODO(Zhao): Support redundant row format
  }
}

void Record::InitColumnOffsetsCompactLeaf() {
  const unsigned char* nulls = nullptr;
  const unsigned char* lens = nullptr;
  uint16_t n_null = 0;
  enum REC_INSERT_STATE rec_insert_state = REC_INSERT_STATE::NONE;
  uint8_t row_version = UINT8_UNDEFINED;
  uint16_t non_default_fields = 0;
  rec_insert_state = InitNullAndLengthCompact(&nulls, &lens, &n_null,
                                 &non_default_fields, &row_version);

  uint32_t offs = 0;
  uint32_t any_ext = 0;
  uint32_t null_mask = 1;
  uint16_t i = 0;
  do {
    IndexColumn* index_col = index_->GetPhysicalField(i);
    Column* col = index_col->column();
    uint64_t len;
    switch (rec_insert_state) {
      case INSERTED_INTO_TABLE_WITH_NO_INSTANT_NO_VERSION:
        assert(!index_->HasInstantColsOrRowVersions());
        break;

      case INSERTED_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION: {
        assert(row_version == UINT8_UNDEFINED || row_version == 0);
        assert(index_->ib_row_versions());
        row_version = 0;
      }
      [[fallthrough]];
      case INSERTED_AFTER_UPGRADE_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION:
      case INSERTED_AFTER_INSTANT_ADD_NEW_IMPLEMENTATION: {
        assert(index_->ib_row_versions() ||
              (index_->table()->ib_m_upgraded_instant() && row_version == 0));

        if (col->IsDroppedInOrBefore(row_version)) {
          len = offs | REC_OFFS_DROP;
          goto resolved;
        } else if (col->IsAddedAfter(row_version)) {
          len = GetInstantOffset(i, offs);
          goto resolved;
        }
      } break;

      case INSERTED_BEFORE_INSTANT_ADD_OLD_IMPLEMENTATION:
      case INSERTED_AFTER_INSTANT_ADD_OLD_IMPLEMENTATION: {
        assert(non_default_fields > 0);
        assert(index_->ib_instant_cols());

        if (i >= non_default_fields) {
          len = GetInstantOffset(i, offs);
          goto resolved;
        }
      } break;

      default:
        assert(false);
    }

    if (col->is_nullable()) {
      assert(n_null--);

      if (!(unsigned char)null_mask) {
        nulls--;
        null_mask = 1;
      }

      if (*nulls & null_mask) {
        null_mask <<= 1;
        len = offs | REC_OFFS_SQL_NULL;
        goto resolved;
      }
      null_mask <<= 1;
    }

    if (!index_col->ib_fixed_len()) {
      /* Variable-length field: read the length */
      len = *lens--;
      if (col->IsBigCol()) {
        if (len & 0x80) {
          len <<= 8;
          len |= *lens--;

          offs += len & 0x3fff;
          if (len & 0x4000) {
            assert(index_->IsClustered());
            any_ext = REC_OFFS_EXTERNAL;
            len = offs | REC_OFFS_EXTERNAL;
          } else {
            len = offs;
          }
          goto resolved;
        }
      }

      len = offs += len;
    } else {
      len = offs += index_col->ib_fixed_len();
    }
  resolved:
    RecOffsBase(offsets_)[i + 1] = len;
  } while (++i < RecOffsNFields(offsets_));

  *RecOffsBase(offsets_) = (rec_ - (lens + 1)) | REC_OFFS_COMPACT | any_ext;
}

#define UT_BITS_IN_BYTES(b) (((b) + 7UL) / 8UL)
Record::REC_INSERT_STATE Record::InitNullAndLengthCompact(
            const unsigned char** nulls, const unsigned char** lens,
            uint16_t* n_null, uint16_t* non_default_fields,
            uint8_t* row_version) {
  *non_default_fields = static_cast<uint16_t>(index_->GetNFields());
  *row_version = UINT8_UNDEFINED;

  *nulls = rec_ - (REC_N_NEW_EXTRA_BYTES + 1);

  const enum REC_INSERT_STATE rec_insert_state =
            GetInsertState();
  switch (rec_insert_state) {
    case INSERTED_INTO_TABLE_WITH_NO_INSTANT_NO_VERSION: {
      assert(!GetInstantFlagCompact());
      assert(!IsVersionedCompact());

      *n_null = index_->ib_n_nullable();
    } break;

    case INSERTED_AFTER_INSTANT_ADD_NEW_IMPLEMENTATION:
    case INSERTED_AFTER_UPGRADE_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION: {
      *row_version = (uint8_t)(**nulls);

      *nulls -= 1;
      *n_null = index_->GetNullableInVersion(*row_version);
    } break;

    case INSERTED_AFTER_INSTANT_ADD_OLD_IMPLEMENTATION: {
      uint16_t length;
      *non_default_fields =
          GetNFieldsInstant(REC_N_NEW_EXTRA_BYTES, &length);
      assert(length == 1 || length == 2);

      *nulls -= length;
      *n_null = index_->CalculateNInstantNullable(*non_default_fields);
    } break;

    case INSERTED_BEFORE_INSTANT_ADD_OLD_IMPLEMENTATION: {
      *n_null = index_->GetNullableBeforeInstantAddDrop();
      *non_default_fields = index_->GetNOriginalFields();
    } break;

    case INSERTED_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION: {
      *n_null = index_->GetNullableBeforeInstantAddDrop();
    } break;

    default:
      assert(0);
  }
  *lens = *nulls - UT_BITS_IN_BYTES(*n_null);

  return (rec_insert_state);
}

Record::REC_INSERT_STATE Record::GetInsertState() {
  if (!index_->HasInstantColsOrRowVersions()) {
    return INSERTED_INTO_TABLE_WITH_NO_INSTANT_NO_VERSION;
  }
  const unsigned char* v_ptr =
      (const unsigned char*)rec_ - (REC_N_NEW_EXTRA_BYTES + 1);
  const bool is_versioned = IsVersionedCompact();
  const uint8_t version = (is_versioned) ? (uint8_t)(*v_ptr) : UINT8_UNDEFINED;
  const bool is_instant = GetInstantFlagCompact();
  assert(!is_versioned || !is_instant);
  enum REC_INSERT_STATE rec_insert_state = REC_INSERT_STATE::NONE;
  if (is_versioned) {
    if (version == 0) {
      assert(index_->ib_instant_cols());
      rec_insert_state =
          INSERTED_AFTER_UPGRADE_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION;
    } else {
      assert(index_->ib_row_versions());
      rec_insert_state = INSERTED_AFTER_INSTANT_ADD_NEW_IMPLEMENTATION;
    }
  } else if (is_instant) {
    assert(index_->table()->HasInstantCols());
    rec_insert_state = INSERTED_AFTER_INSTANT_ADD_OLD_IMPLEMENTATION;
  } else if (index_->table()->HasInstantCols()) {
    rec_insert_state = INSERTED_BEFORE_INSTANT_ADD_OLD_IMPLEMENTATION;
  } else {
    rec_insert_state = INSERTED_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION;
  }

  assert(rec_insert_state != REC_INSERT_STATE::NONE);
  return rec_insert_state;
}

bool Record::IsVersionedCompact() {
  uint32_t info = GetInfoBits(true);
  return ((info & REC_INFO_VERSION_FLAG) != 0);
}
uint32_t Record::GetInfoBits(bool comp) {
  const uint32_t val = GetBitsFrom1B(comp ?
                                     REC_NEW_INFO_BITS : REC_OLD_INFO_BITS,
                                     REC_INFO_BITS_MASK, REC_INFO_BITS_SHIFT);
  return val;
}
bool Record::GetInstantFlagCompact() {
  uint32_t info = GetInfoBits(true);
  return ((info & REC_INFO_INSTANT_FLAG) != 0);
}

uint32_t Record::GetNFieldsInstant(const uint32_t extra_bytes,
                                   uint16_t* length) {
  uint16_t n_fields;
  const unsigned char *ptr;

  ptr = rec_ - (extra_bytes + 1);

  if ((*ptr & REC_N_FIELDS_TWO_BYTES_FLAG) == 0) {
    *length = 1;
    return (*ptr);
  }

  *length = 2;
  n_fields = ((*ptr-- & REC_N_FIELDS_ONE_BYTE_MAX) << 8);
  n_fields |= *ptr;
  assert(n_fields < REC_MAX_N_FIELDS);
  assert(n_fields != 0);

  return (n_fields);
}

uint64_t Record::GetInstantOffset(uint32_t n, uint64_t offs) {
  assert(index_->HasInstantColsOrRowVersions());
  Column* col = index_->GetPhysicalField(n)->column();
  if (col->ib_instant_default()) {
    return (offs | REC_OFFS_DEFAULT);
  } else {
    return (offs | REC_OFFS_SQL_NULL);
  }
}

struct PageAnalysisResult {
  uint32_t n_recs_non_leaf = 0;
  uint32_t n_recs_leaf = 0;
  uint32_t headers_len_non_leaf = 0;
  uint32_t headers_len_leaf = 0;
  uint32_t recs_len_non_leaf = 0;
  uint32_t recs_len_leaf = 0;
  uint32_t n_deleted_recs_non_leaf = 0;
  uint32_t n_deleted_recs_leaf = 0;
  uint32_t deleted_recs_len_non_leaf = 0;
  uint32_t deleted_recs_len_leaf = 0;
  uint32_t n_contain_dropped_cols_recs_non_leaf = 0;  // should always be 0
  uint32_t n_contain_dropped_cols_recs_leaf = 0;
  uint32_t dropped_cols_len_non_leaf = 0;  // should always be 0
  uint32_t dropped_cols_len_leaf = 0;
  uint32_t innodb_internal_used_non_leaf = 0;
  uint32_t innodb_internal_used_leaf = 0;
  uint32_t free_non_leaf = 0;
  uint32_t free_leaf = 0;
};

struct IndexAnalyzeResult {
  uint32_t n_level = 0;
  uint32_t n_pages_non_leaf = 0;
  uint32_t n_pages_leaf = 0;
  PageAnalysisResult recs_result;
};

void Record::ParseRecord(bool leaf, uint32_t row_no,
                         PageAnalysisResult* result,
                         bool print) {
  uint32_t n_fields = leaf ? index_->GetNFields() :
                             index_->GetNUniqueInTreeNonleaf() + 1;
  uint32_t header_len = (RecOffsBase(offsets_)[0] & REC_OFFS_MASK);
  uint32_t rec_len = (RecOffsBase(offsets_)[n_fields] &
                      REC_OFFS_MASK);
  ninja_pt(print, "=========================================="
                  "=============================\n");
  ninja_pt(print, "[ROW %u] Length: %u (%d | %d), Number of fields: %u\n",
                   row_no,
                   header_len + rec_len,
                   header_len,
                   rec_len,
                   n_fields);
  bool deleted = false;
  if (RecGetDeletedFlag(rec_, true) != 0) {
    ninja_pt(print, "[DELETED MARK]\n");
    deleted = true;
  }
  if (!deleted) {
    if (leaf) {
      result->n_recs_leaf++;
      result->headers_len_leaf += header_len;
      result->recs_len_leaf += rec_len;
    } else {
      result->n_recs_non_leaf++;
      result->headers_len_non_leaf += header_len;
      result->recs_len_non_leaf += rec_len;
    }
  } else {
    if (leaf) {
      result->n_deleted_recs_leaf++;
      result->deleted_recs_len_leaf += (header_len + rec_len);
    } else {
      result->n_deleted_recs_non_leaf++;
      result->deleted_recs_len_non_leaf += (header_len + rec_len);
    }
  }
  uint32_t start_pos = 0;
  uint32_t len = 0;
  uint32_t end_pos = 0;
  ibd_ninja::IndexColumn* index_col = nullptr;
  ninja_pt(print, "------------------------------------------"
                  "-----------------------------\n");
  ninja_pt(print, "  [HEADER   ]         ");
  int count = 0;
  for (uint32_t i = 0; i < header_len; i++) {
    ninja_pt(print, "%02x ", (rec_ - header_len)[i]);
    count++;
    if (count == 8) {
      ninja_pt(print, " ");
    } else if (count == 16) {
      ninja_pt(print, "\n                      ");
      count = 0;
    }
  }
  ninja_pt(print, "\n");
  bool dropped_column_counted = false;
  for (uint32_t i = 0; i < n_fields; i++) {
    index_col = nullptr;
    if (!leaf && i == n_fields - 1) {
      ninja_pt(print, "  [FIELD %3u] Name  : *NODE_PTR(Child page no)\n",
                         i + 1);
    } else {
      index_col = index_->GetPhysicalField(i);
      ninja_pt(print, "  [FIELD %3u] Name  : %s\n",
                         i + 1,
                         index_col->column()->name().c_str());
    }

    len = RecOffsBase(offsets_)[i + 1];
    end_pos = (len & REC_OFFS_MASK);
    ninja_pt(print, "              "
                    "Length: %-5u\n",
                    end_pos - start_pos);
    // TODO(Zhao): handle external part
    if (index_col != nullptr &&
        index_col->column()->IsColumnDropped()) {
      // Only count valid records with non-zero size for dropped columns.
      if (!deleted && !(len & REC_OFFS_DROP)) {
        if (leaf) {
          result->dropped_cols_len_leaf += (end_pos - start_pos);
          if (!dropped_column_counted) {
            result->n_contain_dropped_cols_recs_leaf++;
          }
        } else {
          result->dropped_cols_len_non_leaf += (end_pos - start_pos);
          if (!dropped_column_counted) {
            result->n_contain_dropped_cols_recs_non_leaf++;
          }
        }
        dropped_column_counted = true;
      }
    }

    // index_col is nullptr for node_ptr
    if (index_col != nullptr) {
    ninja_pt(print, "              "
                    "Type  : %-15s | %-12s | %-20s\n",
                    index_col->column()->dd_column_type_utf8().c_str(),
                    index_col->column()->FieldTypeString().c_str(),
                    index_col->column()->SeTypeString().c_str());
    }

    ninja_pt(print, "              "
                    "Value : ");
    if (len & REC_OFFS_SQL_NULL) {
      ninja_pt(print, "*NULL*\n");
      start_pos = end_pos;
      continue;
    }
    if (len & REC_OFFS_DROP) {
      ninja_pt(print, "*NULL*\n"
                      "                      "
                      "(This row was inserted after this column "
                      "was instantly dropped)\n");
      start_pos = end_pos;
      continue;
    }
    if (len & REC_OFFS_DEFAULT) {
      ninja_pt(print, "*DEFAULT*\n"
                      "                      "
                      "(This row was inserted before this column "
                      "was instantly added)\n");
      start_pos = end_pos;
      continue;
    }
    count = 0;
    while (start_pos < end_pos) {
      ninja_pt(print, "%02x ", rec_[start_pos]);
      count++;
      if (count == 8) {
        ninja_pt(print, " ");
      } else if (count == 16) {
        ninja_pt(print, "\n                      ");
        count = 0;
      }
      start_pos++;
    }
    if (len & REC_OFFS_EXTERNAL) {
      const unsigned char* ext_ref = &rec_[end_pos - 20];
      [[maybe_unused]] uint32_t space_id =
                         ReadFrom4B(ext_ref + BTR_EXTERN_SPACE_ID);
      [[maybe_unused]] uint32_t page_no =
                         ReadFrom4B(ext_ref + BTR_EXTERN_PAGE_NO);
      uint64_t ext_len = ReadFrom4B(ext_ref + BTR_EXTERN_LEN + 4);
      ninja_pt(print, "\n                      "
              "[Remaining %" PRIu64 " bytes have been offloaded externally...]",
              ext_len);
    }
    ninja_pt(print, "\n");
  }
}

uint32_t Record::GetChildPageNo() {
  uint32_t n_fields = GetNFields();
  assert(n_fields >= 2);
  uint32_t last_2_len = (RecOffsBase(offsets_)[n_fields - 1]);
  uint32_t last_2_end_pos = (last_2_len & REC_OFFS_MASK);
  uint32_t last_len = (RecOffsBase(offsets_)[n_fields]);
  uint32_t last_end_pos = (last_len & REC_OFFS_MASK);
  assert(last_end_pos - last_2_end_pos == 4);
  return ReadFrom4B(&rec_[last_2_end_pos]);
}

/* ------ Ninja ------ */
static bool ValidateSDI(const rapidjson::Document& doc) {
  bool ret = true;
  if (!doc.HasMember("dd_object_type") ||
      !doc["dd_object_type"].IsString() ||
      (std::string(doc["dd_object_type"].GetString()) != "Table" &&
      (std::string(doc["dd_object_type"].GetString()) != "Tablespace")) ||
      !doc.HasMember("dd_object") ||
      !doc["dd_object"].IsObject()) {
    ret = false;
  }

  if (!doc.HasMember("mysqld_version_id") ||
      !doc["mysqld_version_id"].IsUint() ||
      !doc.HasMember("dd_version") ||
      !doc["dd_version"].IsUint() ||
      !doc.HasMember("sdi_version") ||
      !doc["sdi_version"].IsUint()) {
    ret = false;
  }
  return ret;
}

const char* ibdNinja::g_version_ = "1.0.0";

void ibdNinja::PrintName() {
  fprintf(stdout,
"|--------------------------------------------------------------------------------------------------------------|\n"
"|    _      _                         _   _           _      _                              _                  |\n"
"|   (_)    (_)                       (_) (_) _       (_)    (_)                            (_)                 |\n"
"| _  _     (_) _  _  _       _  _  _ (_) (_)(_)_     (_)  _  _      _  _  _  _           _  _     _  _  _      |\n"
"|(_)(_)    (_)(_)(_)(_)_   _(_)(_)(_)(_) (_)  (_)_   (_) (_)(_)    (_)(_)(_)(_)_        (_)(_)   (_)(_)(_) _   |\n"
"|   (_)    (_)        (_) (_)        (_) (_)    (_)_ (_)    (_)    (_)        (_)          (_)    _  _  _ (_)  |\n"
"|   (_)    (_)        (_) (_)        (_) (_)      (_)(_)    (_)    (_)        (_)          (_)  _(_)(_)(_)(_)  |\n"
"| _ (_) _  (_) _  _  _(_) (_)_  _  _ (_) (_)         (_)  _ (_) _  (_)        (_)          (_) (_)_  _  _ (_)_ |\n"
"|(_)(_)(_) (_)(_)(_)(_)     (_)(_)(_)(_) (_)         (_) (_)(_)(_) (_)        (_)  _      _(_)   (_)(_)(_)  (_)|\n"
"|                                                                                 (_)_  _(_)                   |\n"
"|                                                                                   (_)(_)                     |\n"
"|--------------------------------------------------------------------------------------------------------------|\n");
}

ibdNinja* ibdNinja::CreateNinja(const char* ibd_filename) {
  unsigned char buf[UNIV_PAGE_SIZE_MAX];
  memset(buf, 0, UNIV_PAGE_SIZE_MAX);
  struct stat stat_info;
  if (stat(ibd_filename, &stat_info) != 0) {
    ninja_error("Failed to get file stats: %s, error: %d(%s)",
            ibd_filename, errno, strerror(errno));
    return nullptr;
  }
  uint64_t size = stat_info.st_size;
  g_fd = open(ibd_filename, O_RDONLY);
  if (g_fd == -1) {
    ninja_error("Failed to open file: %s, error: %d(%s)",
            ibd_filename, errno, strerror(errno));
    close(g_fd);
    return nullptr;
  }
  if (size < UNIV_ZIP_SIZE_MIN) {
    ninja_error("The file is too small to be a valid ibd file");
    close(g_fd);
    return nullptr;
  }
  ssize_t bytes = read(g_fd, buf, UNIV_ZIP_SIZE_MIN);
  if (bytes != UNIV_ZIP_SIZE_MIN) {
    ninja_error("Failed to read file header: %s, error: %d(%s)",
            ibd_filename, errno, strerror(errno));
    close(g_fd);
    return nullptr;
  }
  uint32_t space_id = ReadFrom4B(buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
  uint32_t first_page_no = ReadFrom4B(buf + FIL_PAGE_OFFSET);
  uint32_t flags = FSPHeaderGetFlags(buf);
  bool is_valid_flags = FSPFlagsIsValid(flags);
  uint32_t page_size = 0;
  if (is_valid_flags) {
    uint32_t ssize = FSP_FLAGS_GET_PAGE_SSIZE(flags);
    if (ssize == 0) {
      page_size = UNIV_PAGE_SIZE_ORIG;
    } else {
      page_size = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);
    }
    g_page_size_shift = PageSizeValidate(page_size);
  }
  if (!is_valid_flags || g_page_size_shift == 0) {
    ninja_error("Found corruption on page 0 of file %s",
            ibd_filename);
    close(g_fd);
    return nullptr;
  }

  g_page_logical_size = page_size;

  assert(g_page_logical_size <= UNIV_PAGE_SIZE_MAX);
  assert(g_page_logical_size <= (1 << PAGE_SIZE_T_SIZE_BITS));

  uint32_t ssize = FSP_FLAGS_GET_ZIP_SSIZE(flags);

  if (ssize == 0) {
    g_page_compressed = false;
    g_page_physical_size = g_page_logical_size;
  } else {
    g_page_compressed = true;

    g_page_physical_size = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);

    assert(g_page_physical_size <= UNIV_ZIP_SIZE_MAX);
    assert(g_page_physical_size <= (1 << PAGE_SIZE_T_SIZE_BITS));
  }
  uint32_t n_pages = size / g_page_physical_size;

  uint32_t post_antelope = FSP_FLAGS_GET_POST_ANTELOPE(flags);
  uint32_t atomic_blobs = FSP_FLAGS_HAS_ATOMIC_BLOBS(flags);
  uint32_t has_data_dir = FSP_FLAGS_HAS_DATA_DIR(flags);
  uint32_t shared = FSP_FLAGS_GET_SHARED(flags);
  uint32_t temporary = FSP_FLAGS_GET_TEMPORARY(flags);
  uint32_t encryption = FSP_FLAGS_GET_ENCRYPTION(flags);
  uint32_t has_sdi = FSP_FLAGS_HAS_SDI(flags);

  bytes = ReadPage(0, buf);
  if (bytes == -1) {
    ninja_error("Failed to read file header: %s, error: %d(%s)",
            ibd_filename, errno, strerror(errno));
    close(g_fd);
    return nullptr;
  }
  uint32_t sdi_offset = XDES_ARR_OFFSET +
                    XDES_SIZE * (g_page_physical_size / FSP_EXTENT_SIZE) +
                    INFO_MAX_SIZE;
  assert(sdi_offset + 4 < bytes);
  uint32_t sdi_root = ReadFrom4B(buf + sdi_offset + 4);
  if (!has_sdi) {
    ninja_warn("FSP doesn't have SDI flags... "
               "Attempting to parse the SDI root page %u directly anyway.",
               sdi_root);
  }
  fprintf(stdout, "=========================================="
                  "==========================================\n");
  fprintf(stdout, "|  FILE INFORMATION                       "
                  "                                         |\n");
  fprintf(stdout, "------------------------------------------"
                  "------------------------------------------\n");
  fprintf(stdout, "    File name:             %s\n", ibd_filename);
  fprintf(stdout, "    File size:             %" PRIu64 " B\n", size);
  fprintf(stdout, "    Space id:              %u\n", space_id);
  fprintf(stdout, "    Page logical size:     %u B\n", g_page_logical_size);
  fprintf(stdout, "    Page physical size:    %u B\n", g_page_physical_size);
  fprintf(stdout, "    Total number of pages: %u\n", n_pages);
  fprintf(stdout, "    Is compressed page?    %u\n", g_page_compressed);
  fprintf(stdout, "    First page number:     %u\n", first_page_no);
  fprintf(stdout, "    SDI root page number:  %u\n", sdi_root);
  fprintf(stdout, "    Post antelop:          %u\n", post_antelope);
  fprintf(stdout, "    Atomic blobs:          %u\n", atomic_blobs);
  fprintf(stdout, "    Has data dir:          %u\n", has_data_dir);
  fprintf(stdout, "    Shared:                %u\n", shared);
  fprintf(stdout, "    Temporary:             %u\n", temporary);
  fprintf(stdout, "    Encryption:            %u\n", encryption);
  fprintf(stdout, "------------------------------------------"
                  "------------------------------------------\n");

  if (g_page_compressed) {
    ninja_error("Parsing of compressed table/tablespaces is "
                "not yet supported.");
    return nullptr;
  }
  if (encryption) {
    ninja_error("Parsing of encrpted space is not yet supported");
    return nullptr;
  }
  if (temporary) {
    ninja_error("Parsing of temporary space is not yet supported");
    return nullptr;
  }

  /* DEBUG
  fprintf(stdout, "[ibdNinja]: Loading SDI...\n"
                  "            1. Traversaling down to the "
                  "leafmost leaf page\n");
  */
  unsigned char buf_unalign[2 * UNIV_PAGE_SIZE_MAX];
  memset(buf_unalign, 0, 2 * UNIV_PAGE_SIZE_MAX);
  unsigned char* buf_align = static_cast<unsigned char*>(
                    ut_align(buf_unalign, g_page_physical_size));
  uint32_t leaf_page_no = 0;
  bool res = SDIToLeftmostLeaf(buf_align, sdi_root, &leaf_page_no);
  if (!res) {
    return nullptr;
  }

  /* DEBUG
  fprintf(stdout, "            2. Parsing SDI records and loading tables:\n");
  */
  unsigned char* current_rec = SDIGetFirstUserRec(buf_align,
                                                  g_page_physical_size);
  if (current_rec == nullptr) {
    return nullptr;
  }
  ibdNinja* ninja = new ibdNinja(n_pages);
  bool corrupt = false;
  uint64_t sdi_id = 0;
  uint64_t sdi_type = 0;
  unsigned char* sdi_data = nullptr;
  uint64_t sdi_data_len = 0;
  while (current_rec != nullptr && !corrupt) {
    bool ret = SDIParseRec(current_rec, &sdi_type, &sdi_id, &sdi_data, &sdi_data_len);
    if (ret == false) {
      corrupt = true;
      break;
    }
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(
                                reinterpret_cast<const char*>(sdi_data));
    if (!ok) {
      std::cerr << "JSON parse error: "
                << rapidjson::GetParseError_En(ok.Code()) << " (offset "
                << ok.Offset() << ")"
                << " sdi: " << sdi_data << std::endl;
      delete[] sdi_data;
      corrupt = true;
      delete ninja;
      return nullptr;
    }
    if (!ValidateSDI(doc)) {
      std::cerr << "Invalid SDI: " << sdi_data << std::endl;
      delete[] sdi_data;
      corrupt = true;
      delete ninja;
      return nullptr;
    }

    [[maybe_unused]] uint32_t mysqld_version_id =
                                doc["mysqld_version_id"].GetUint();
    [[maybe_unused]] uint32_t dd_version =
                                doc["dd_version"].GetUint();
    [[maybe_unused]] uint32_t sdi_version =
                                doc["sdi_version"].GetUint();

    if (std::string(doc["dd_object_type"].GetString()) == "Table") {
      const rapidjson::Value& dd_object = doc["dd_object"];
      Table* table = Table::CreateTable(dd_object, sdi_data);
      if (table != nullptr) {
        // table->DebugDump();
        ninja->AddTable(table);
        /* DEBUG
           fprintf(stdout, "              %s.%s\n",
           table->schema_ref().c_str(),
           table->name().c_str());
         */
      } else {
        ninja_warn("Failed to recover table %s from SDI, "
                    "the SDI may be corrupt, skipping it",
                    dd_object["name"].GetString());
      }
    } else {
      // Tablespace
      delete[] sdi_data;
    }

    current_rec = SDIGetNextRec(current_rec, buf_align,
                            g_page_physical_size, &corrupt);
  }
  if (corrupt) {
    delete ninja;
    return nullptr;
  }
  fprintf(stdout, "[ibdNinja]: Successfully loaded %5lu tables "
                  "with %5lu indexes.\n",
          ninja->tables()->size(), ninja->indexes()->size());
  fprintf(stdout, "=========================================="
                  "==========================================\n\n");
  return ninja;
}

void ibdNinja::AddTable(Table* table) {
  all_tables_.push_back(table);
  if (!table->IsTableSupported()) {
    ninja_warn("Skipping loading table '%s.%s', Reason: '%s'",
               table->schema_ref().c_str(), table->name().c_str(),
               table->UnsupportedReason().c_str());
    return;
  }
  tables_.insert({table->se_private_id(), table});
  for (auto iter : table->indexes()) {
    if (!iter->IsIndexSupported()) {
      ninja_warn("Skipping loading index '%s' of table '%s.%s', "
                 "Reason: '%s'\n",
                 iter->name().c_str(),
                 table->schema_ref().c_str(), table->name().c_str(),
                 iter->UnsupportedReason().c_str());
      continue;
    }
    uint64_t index_id = 0;
    assert(iter->se_private_data().Exists("id"));
    iter->se_private_data().Get("id", &index_id);
    indexes_.insert({index_id, iter});
  }
}

bool ibdNinja::SDIToLeftmostLeaf(unsigned char* buf, uint32_t sdi_root,
                                 uint32_t* leaf_page_no) {
  uint32_t bytes = ReadPage(sdi_root, buf);
  if (bytes != g_page_physical_size) {
    ninja_error("Failed to read page: %u, error: %d(%s)",
            sdi_root, errno, strerror(errno));
    return false;
  }
  uint32_t page_level = ReadFrom2B(buf + FIL_PAGE_DATA + PAGE_LEVEL);
  uint32_t n_of_recs = ReadFrom2B(buf + FIL_PAGE_DATA + PAGE_N_RECS);
  if (n_of_recs == 0) {
    ninja_warn("No SDI is found in this file, "
               "it might be from an older MySQL version.");
    ninja_warn("ibdNinja currently supports MySQL 8.0.16 to 8.0.40.");
    return false;
  }

  uint32_t curr_page_no = sdi_root;
  unsigned char rec_type_byte;
  unsigned char rec_type;
  /* DEBUG
  fprintf(stdout, "              Level %u: page %u\n",
                  page_level, curr_page_no);
  */
  while (page_level != 0) {
    rec_type_byte = *(buf + PAGE_NEW_INFIMUM - REC_OFF_TYPE);

    rec_type = rec_type_byte & 0x7;

    if (rec_type != REC_STATUS_INFIMUM) {
      ninja_error("Failed to get INFIMUM from page: %u", curr_page_no);
      break;
    }

    uint32_t next_rec_off_t =
        ReadFrom2B(buf + PAGE_NEW_INFIMUM - REC_OFF_NEXT);

    uint32_t child_page_no =
        ReadFrom4B(buf + PAGE_NEW_INFIMUM + next_rec_off_t +
                         REC_DATA_TYPE_LEN + REC_DATA_ID_LEN);

    if (child_page_no < SDI_BLOB_ALLOWED) {
      ninja_error("Failed to get INFIMUM from page: %u", child_page_no);
      return false;
    }

    uint64_t curr_page_level = page_level;

    bytes = ReadPage(child_page_no, buf);
    if (bytes != g_page_physical_size) {
      ninja_error("Failed to read page: %u, error: %d(%s)",
              child_page_no, errno, strerror(errno));
      return false;
    }
    page_level = ReadFrom2B(buf + FIL_PAGE_DATA + PAGE_LEVEL);
    n_of_recs = ReadFrom2B(buf + FIL_PAGE_DATA + PAGE_N_RECS);

    if (page_level != curr_page_level - 1) {
      break;
    }
    curr_page_no = child_page_no;
    /* DEBUG
    fprintf(stdout, "              Level %u: page %u\n",
                    page_level, curr_page_no);
    */
  }

  if (page_level != 0) {
    ninja_error("Failed to find the leftmost page. "
                "The page may be compressed or corrupted\n");

    return false;
  }
  *leaf_page_no = curr_page_no;
  return true;
}

unsigned char* ibdNinja::SDIGetFirstUserRec(unsigned char* buf,
                                            uint32_t buf_len) {
  uint32_t next_rec_off_t =
            ReadFrom2B(buf + PAGE_NEW_INFIMUM - REC_OFF_NEXT);

  assert(PAGE_NEW_INFIMUM + next_rec_off_t != PAGE_NEW_SUPREMUM);

  if (next_rec_off_t > buf_len) {
    assert(0);
    return (nullptr);
  }

  if (memcmp(buf + PAGE_NEW_INFIMUM, "infimum", strlen("infimum")) != 0) {
    ninja_error("Found corrupt INFIMUM");
    return nullptr;
  }

  unsigned char* current_rec = buf + PAGE_NEW_INFIMUM + next_rec_off_t;

  assert(static_cast<uint32_t>(current_rec - buf) <= buf_len);

  bool is_comp = PageIsCompact(buf);

  // TODO(Zhao): Support redundant row format
  assert(is_comp);
  /* record is delete marked, get next record */
  if (RecGetDeletedFlag(current_rec, is_comp) != 0) {
    bool corrupt;
    current_rec =
        SDIGetNextRec(current_rec, buf, buf_len, &corrupt);
    if (corrupt) {
      return nullptr;
    }
  }

  return current_rec;
}

unsigned char* ibdNinja::SDIGetNextRec(unsigned char* current_rec,
                                       unsigned char* buf,
                                       uint32_t buf_len,
                                       bool* corrupt) {
  *corrupt = false;
  uint32_t page_no = ReadFrom4B(buf + FIL_PAGE_OFFSET);
  bool is_comp = PageIsCompact(buf);
  uint32_t next_rec_offset = RecGetNextOffs(current_rec, is_comp);

  if (next_rec_offset == 0) {
    ninja_error("Record is corrupt");
    *corrupt = true;
    return nullptr;
  }

  unsigned char* next_rec = buf + next_rec_offset;

  assert(static_cast<uint32_t>(next_rec - buf) <= buf_len);

  if (RecGetDeletedFlag(next_rec, is_comp) != 0) {
    unsigned char* curr_rec = next_rec;
    return SDIGetNextRec(curr_rec, buf, buf_len, corrupt);
  }

  if (RecGetType(next_rec) == REC_STATUS_SUPREMUM) {
    if (memcmp(next_rec, "supremum", strlen("supremum")) != 0) {
      ninja_error("Found corrupt SUPREMUM on page %u", page_no);
      *corrupt = false;
      return nullptr;
    }

    uint32_t supremum_next_rec_off = ReadFrom2B(next_rec - REC_OFF_NEXT);

    if (supremum_next_rec_off != 0) {
      ninja_error("Found corrupt next of SUPREMUM on page %u", page_no);
      *corrupt = false;
      return nullptr;
    }

    uint32_t next_page_no = ReadFrom4B(buf + FIL_PAGE_NEXT);

    if (next_page_no == FIL_NULL) {
      *corrupt = false;
      return nullptr;
    }

    uint32_t bytes = ReadPage(next_page_no, buf);
    if (bytes != g_page_physical_size) {
      ninja_error("Failed to read page: %u, error: %d(%s)",
              next_page_no, errno, strerror(errno));
      *corrupt = true;
      return nullptr;
    }

    uint16_t page_type = PageGetType(buf);

    if (page_type != FIL_PAGE_SDI) {
      ninja_error("Unexpected page type: %u (%u)",
              page_type, FIL_PAGE_SDI);
      *corrupt = true;
      return nullptr;
    }

    next_rec = SDIGetFirstUserRec(buf, buf_len);
  }

  *corrupt = false;

  return next_rec;
}

bool ibdNinja::SDIParseRec(unsigned char* rec,
                        uint64_t* sdi_type, uint64_t* sdi_id,
                        unsigned char** sdi_data, uint64_t* sdi_data_len) {
  if (RecIsInfimum(rec) || RecIsSupremum(rec)) {
    return false;
  }

  *sdi_type = ReadFrom4B(rec + REC_OFF_DATA_TYPE);
  *sdi_id = ReadFrom8B(rec + REC_OFF_DATA_ID);
  uint32_t sdi_uncomp_len = ReadFrom4B(rec + REC_OFF_DATA_UNCOMP_LEN);
  uint32_t sdi_comp_len = ReadFrom4B(rec + REC_OFF_DATA_COMP_LEN);

  unsigned rec_data_len_partial = *(rec - REC_MIN_HEADER_SIZE - 1);

  uint64_t rec_data_length;
  bool is_rec_data_external = false;
  uint32_t rec_data_in_page_len = 0;

  if (rec_data_len_partial & 0x80) {
    rec_data_in_page_len = (rec_data_len_partial & 0x3f) << 8;
    if (rec_data_len_partial & 0x40) {
      is_rec_data_external = true;
      rec_data_length =
          ReadFrom8B(rec + REC_OFF_DATA_VARCHAR + rec_data_in_page_len +
                           BTR_EXTERN_LEN);

      rec_data_length += rec_data_in_page_len;
    } else {
      rec_data_length = *(rec - REC_MIN_HEADER_SIZE - 2);
      rec_data_length += rec_data_in_page_len;
    }
  } else {
    rec_data_length = rec_data_len_partial;
  }

  unsigned char* str = new unsigned char[rec_data_length + 1]();

  unsigned char* rec_data_origin = rec + REC_OFF_DATA_VARCHAR;

  if (is_rec_data_external) {
    assert(rec_data_in_page_len == 0 ||
          rec_data_in_page_len == REC_ANTELOPE_MAX_INDEX_COL_LEN);

    if (rec_data_in_page_len != 0) {
      memcpy(str, rec_data_origin, rec_data_in_page_len);
    }

    /* Copy from off-page blob-pages */
    uint32_t first_blob_page_no =
        ReadFrom4B(rec + REC_OFF_DATA_VARCHAR + rec_data_in_page_len +
                         BTR_EXTERN_PAGE_NO);

    uint64_t blob_len_retrieved = 0;
    if (g_page_compressed) {
      // TODO(Zhao): Support compressed page
    } else {
      uint32_t n_ext_pages = 0;
      bool error = false;
      blob_len_retrieved = SDIFetchUncompBlob(
          first_blob_page_no, rec_data_length - rec_data_in_page_len,
          str + rec_data_in_page_len, &n_ext_pages, &error);
    }
    *sdi_data_len = rec_data_in_page_len + blob_len_retrieved;
  } else {
    memcpy(str, rec_data_origin, static_cast<size_t>(rec_data_length));
    *sdi_data_len = rec_data_length;
  }

  *sdi_data_len = rec_data_length;
  *sdi_data = str;

  assert(rec_data_length == sdi_comp_len);

  if (rec_data_length != sdi_comp_len) {
    /* Record Corruption */
    ninja_error("SDI record corruption");
    delete[] str;
    return false;
  }

  unsigned char* uncompressed_sdi = new unsigned char[sdi_uncomp_len + 1]();
  int ret;
  uLongf dest_len = sdi_uncomp_len;
  ret = uncompress(uncompressed_sdi, &dest_len,
                   str, sdi_comp_len);

  if (ret != Z_OK) {
    ninja_error("Failed to uncompress SDI record, error: %d", ret);
    delete[] str;
    return false;
  }

  *sdi_data_len = sdi_uncomp_len + 1;
  *sdi_data = uncompressed_sdi;
  delete[] str;

  return true;
}

uint64_t ibdNinja::SDIFetchUncompBlob(uint32_t first_blob_page_no,
                                      uint64_t total_off_page_length,
                                      unsigned char* dest_buf,
                                      uint32_t* n_ext_pages,
                                      bool* error) {
  unsigned char page_buf[UNIV_PAGE_SIZE_MAX];
  uint64_t calc_length = 0;
  uint64_t part_len;
  uint32_t next_page_no = first_blob_page_no;
  *error = false;
  *n_ext_pages = 0;

  do {
    uint32_t bytes = ReadPage(next_page_no, page_buf);
    *n_ext_pages += 1;
    if (bytes != g_page_physical_size) {
      ninja_error("Failed to read BLOB page: %u, error: %d(%s)",
              next_page_no, errno, strerror(errno));
      *error = true;
      break;
    }

    if (PageGetType(page_buf) != FIL_PAGE_SDI_BLOB) {
      ninja_error("Unexpected BLOB page type: %u (%u)",
                      PageGetType(page_buf), FIL_PAGE_SDI_BLOB);
      *error = true;
      break;
    }

    part_len =
        ReadFrom4B(page_buf + FIL_PAGE_DATA + LOB_HDR_PART_LEN);

    if (dest_buf) {
      memcpy(dest_buf + calc_length, page_buf + FIL_PAGE_DATA + LOB_HDR_SIZE,
          static_cast<size_t>(part_len));
    }

    calc_length += part_len;

    next_page_no =
        ReadFrom4B(page_buf + FIL_PAGE_DATA + LOB_HDR_NEXT_PAGE_NO);

    if (next_page_no <= SDI_BLOB_ALLOWED) {
      ninja_error("Failed to get next BLOB page: %u", next_page_no);
      *error = true;
      break;
    }
  } while (next_page_no != FIL_NULL);

  if (!*error) {
    assert(calc_length == total_off_page_length);
  }
  return calc_length;
}

unsigned char* ibdNinja::GetFirstUserRec(unsigned char* buf) {
  uint32_t next_rec_off_t =
            ReadFrom2B(buf + PAGE_NEW_INFIMUM - REC_OFF_NEXT);

  assert(PAGE_NEW_INFIMUM + next_rec_off_t != PAGE_NEW_SUPREMUM);

  if (next_rec_off_t > g_page_physical_size) {
    assert(0);
    return (nullptr);
  }

  if (memcmp(buf + PAGE_NEW_INFIMUM, "infimum", strlen("infimum")) != 0) {
    ninja_error("Found corrupt INFIMUM");
    return nullptr;
  }

  unsigned char* current_rec = buf + PAGE_NEW_INFIMUM + next_rec_off_t;

  assert(static_cast<uint32_t>(current_rec - buf) <= g_page_physical_size);

  bool is_comp = PageIsCompact(buf);

  // TODO(Zhao): Support redundant row format
  assert(is_comp);

  return current_rec;
}

unsigned char* ibdNinja::GetNextRecInPage(unsigned char* current_rec,
                                          unsigned char* buf,
                                          bool* corrupt) {
  *corrupt = false;
  uint32_t page_no = ReadFrom4B(buf + FIL_PAGE_OFFSET);
  bool is_comp = PageIsCompact(buf);
  uint32_t next_rec_offset = RecGetNextOffs(current_rec, is_comp);

  if (next_rec_offset == 0) {
    ninja_error("Record is corrupt");
    *corrupt = true;
    assert(0);
    return nullptr;
  }

  unsigned char* next_rec = buf + next_rec_offset;

  assert(static_cast<uint32_t>(next_rec - buf) <= g_page_physical_size);

  if (RecGetType(next_rec) == REC_STATUS_SUPREMUM) {
    if (memcmp(next_rec, "supremum", strlen("supremum")) != 0) {
      ninja_error("Found corrupt SUPREMUM on page %u", page_no);
      *corrupt = false;
    }

    uint32_t supremum_next_rec_off = ReadFrom2B(next_rec - REC_OFF_NEXT);

    if (supremum_next_rec_off != 0) {
      ninja_error("Found corrupt next rec of SUPREMUM on page %u", page_no);
      *corrupt = false;
    }
    return nullptr;
  }

  *corrupt = false;

  return next_rec;
}

ssize_t ibdNinja::ReadPage(uint32_t page_no, unsigned char* buf) {
  assert(buf != nullptr);
  memset(buf, 0, g_page_physical_size);
  off_t offset = page_no * g_page_physical_size;
  ssize_t n_bytes_read = pread(g_fd, buf, g_page_physical_size, offset);

  // TODO(Zhao): Support compressed page
  return n_bytes_read;
}

bool ibdNinja::ParsePage(uint32_t page_no,
                         PageAnalysisResult* result_aggr,
                         bool print,
                         bool print_record) {
  if (page_no >= n_pages_) {
    ninja_error("Page number %u is too large", page_no);
    return false;
  }

  unsigned char buf_unalign[2 * UNIV_PAGE_SIZE_MAX];
  memset(buf_unalign, 0, 2 * UNIV_PAGE_SIZE_MAX);
  unsigned char* buf = static_cast<unsigned char*>(
                    ut_align(buf_unalign, g_page_physical_size));
  ssize_t bytes = ReadPage(page_no, buf);
  if (bytes != g_page_physical_size) {
    ninja_error("Failed to read page: %u, error: %d(%s)",
            page_no, errno, strerror(errno));
    return false;
  }
  if (memcmp(
          buf + FIL_PAGE_LSN + 4,
          buf + g_page_logical_size - FIL_PAGE_END_LSN_OLD_CHKSUM + 4,
          4)) {
    ninja_error("The LSN on page %u is inconsistent", page_no);
    return false;
  }

  uint32_t type = ReadFrom2B(buf + FIL_PAGE_TYPE);
  if (type != FIL_PAGE_INDEX) {
    fprintf(stderr, "[ibdNinja] Currently, only INDEX pages are supported. "
                "Support for other types (e.g., '%s') will be added soon\n",
                PageType2String(type).c_str());
    return false;
  }

  uint32_t page_no_in_fil_header = ReadFrom4B(buf + FIL_PAGE_OFFSET);
  assert(page_no_in_fil_header == page_no);
  uint32_t prev_page_no = ReadFrom4B(buf + FIL_PAGE_PREV);
  uint32_t next_page_no = ReadFrom4B(buf + FIL_PAGE_NEXT);
  uint32_t space_id = ReadFrom4B(buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
  uint32_t lsn = ReadFrom8B(buf + FIL_PAGE_LSN);
  uint32_t flush_lsn = ReadFrom8B(buf + FIL_PAGE_FILE_FLUSH_LSN);

  uint32_t n_dir_slots = ReadFrom2B(buf + PAGE_HEADER + PAGE_N_DIR_SLOTS);
  uint32_t heap_top = ReadFrom2B(buf + PAGE_HEADER + PAGE_HEAP_TOP);
  uint32_t n_heap = ReadFrom2B(buf + PAGE_HEADER + PAGE_N_HEAP);
  n_heap = (n_heap & 0x7FFF);
  uint32_t free = ReadFrom2B(buf + PAGE_HEADER + PAGE_FREE);
  uint32_t garbage = ReadFrom2B(buf + PAGE_HEADER + PAGE_GARBAGE);
  uint32_t last_insert = ReadFrom2B(buf + PAGE_HEADER + PAGE_LAST_INSERT);
  uint32_t direction = ReadFrom2B(buf + PAGE_HEADER + PAGE_DIRECTION);
  uint32_t n_direction = ReadFrom2B(buf + PAGE_HEADER + PAGE_N_DIRECTION);
  uint32_t n_recs = ReadFrom2B(buf + PAGE_HEADER + PAGE_N_RECS);
  uint32_t max_trx_id = ReadFrom2B(buf + PAGE_HEADER + PAGE_MAX_TRX_ID);
  uint32_t page_level = ReadFrom2B(buf + PAGE_HEADER + PAGE_LEVEL);
  uint64_t index_id = ReadFrom8B(buf + PAGE_HEADER + PAGE_INDEX_ID);

  bool index_not_found = false;
  Index* index = GetIndex(index_id);
  if (index == nullptr) {
    ninja_error("Unable find index %" PRIu64 " in the loaded indexes",
            index_id);
    index_not_found = true;
  }

  ninja_pt(print, "=========================================="
                  "==========================================\n");
  ninja_pt(print, "|  PAGE INFORMATION                       "
                  "                                         |\n");
  ninja_pt(print, "------------------------------------------"
                  "------------------------------------------\n");
  ninja_pt(print, "    Page no:           %u\n", page_no);
  if (prev_page_no != FIL_NULL) {
    ninja_pt(print, "    Slibling pages no: %u ", prev_page_no);
  } else {
    ninja_pt(print, "    Slibling pages no: NULL ");
  }
  ninja_pt(print, "[%u] ", page_no_in_fil_header);
  if (next_page_no != FIL_NULL) {
    ninja_pt(print, "%u\n", next_page_no);
  } else {
    ninja_pt(print, "NULL\n");
  }
  ninja_pt(print, "    Space id:          %u\n", space_id);
  ninja_pt(print, "    Page type:         %s\n", PageType2String(type).c_str());
  ninja_pt(print, "    Lsn:               %u\n", lsn);
  ninja_pt(print, "    FLush lsn:         %u\n", flush_lsn);
  ninja_pt(print, "    -------------------\n");
  ninja_pt(print, "    Page level:        %u\n", page_level);
  ninja_pt(print, "    Page size:         [logical: %u B], [physical: %u B]\n",
                       g_page_logical_size,
                       g_page_physical_size);
  ninja_pt(print, "    Number of records: %u\n", n_recs);
  ninja_pt(print, "    Index id:          %" PRIu64 "\n", index_id);
  if (!index_not_found) {
  ninja_pt(print, "    Belongs to:        [table: %s.%s], [index: %s]\n",
                       index->table()->schema_ref().c_str(),
                       index->table()->name().c_str(),
                       index->name().c_str());
  ninja_pt(print, "    Row format:        %s\n",
                       index->table()->RowFormatString().c_str());
  }
  ninja_pt(print, "    Number dir slots:  %u\n", n_dir_slots);
  ninja_pt(print, "    Heap top:          %u\n", heap_top);
  ninja_pt(print, "    Number of heap:    %u\n", n_heap);
  ninja_pt(print, "    First free rec:    %u\n", free);
  ninja_pt(print, "    Garbage:           %u B\n", garbage);
  ninja_pt(print, "    Last insert:       %u\n", last_insert);
  ninja_pt(print, "    Direction:         %u\n", direction);
  ninja_pt(print, "    Number direction:  %u\n", n_direction);
  ninja_pt(print, "    Max trx id:        %u\n", max_trx_id);

  ninja_pt(print, "\n");

  if (index_not_found) {
    ninja_warn("Skipping record parsing");
    return false;
  }

  if (!index->IsIndexParsingRecSupported()) {
    ninja_warn("Skipping record parsing");
    return false;
  }

  bool print_rec = print & print_record;
  ninja_pt(print_rec, "=========================================="
                  "==========================================\n");
  ninja_pt(print_rec, "|  RECORDS INFORMATION                    "
                  "                                         |\n");
  ninja_pt(print_rec, "------------------------------------------"
                  "------------------------------------------\n");
  uint32_t i = 0;
  unsigned char* current_rec = nullptr;
  PageAnalysisResult result;
  if (n_recs > 0) {
    current_rec = GetFirstUserRec(buf);
    bool corrupt = false;
    while (current_rec != nullptr && corrupt != true) {
      i++;
      Record rec(current_rec, index);
      rec.GetColumnOffsets();
      rec.ParseRecord(page_level == 0, i, &result,
                      print_rec);
      current_rec = GetNextRecInPage(current_rec, buf, &corrupt);
    }
    if (corrupt != true) {
      assert(i == n_recs);
    }
  } else {
    ninja_pt(print_rec, "No record\n");
  }


  ninja_pt(print, "=========================================="
      "==========================================\n");
  ninja_pt(print, "|  PAGE ANALYSIS RESULT                    "
      "                                         |\n");
  ninja_pt(print, "------------------------------------------"
      "------------------------------------------\n");
  if (page_level == 0) {
    ninja_pt(print, "Total valid records count:                %u\n",
        result.n_recs_leaf);
    ninja_pt(print, "Total valid records size:                 %u B\n"
        "                                            "
        "[Headers: %u B]\n"
        "                                            "
        "[Bodies:  %u B]\n",
        result.headers_len_leaf + result.recs_len_leaf,
        result.headers_len_leaf, result.recs_len_leaf);
    ninja_pt(print, "Valid records to page space ratio:        "
        "%02.05lf %%\n",
        static_cast<double>(
          (result.headers_len_leaf +
           result.recs_len_leaf)) /
        g_page_physical_size * 100);

    ninja_pt(print, "\n");
    ninja_pt(print, "Total records with dropped columns count: %u\n",
        result.n_contain_dropped_cols_recs_leaf);
    ninja_pt(print, "Total instant dropped columns size:       %u B\n",
        result.dropped_cols_len_leaf);
    ninja_pt(print, "Dropped columns to page space ratio:      "
        "%02.05lf %%\n",
        static_cast<double>(
          result.dropped_cols_len_leaf) /
        g_page_physical_size * 100);

    ninja_pt(print, "\n");
    ninja_pt(print, "Total delete-marked records count:        %u\n",
        result.n_deleted_recs_leaf);
    ninja_pt(print, "Total delete-marked records size:         %u B\n",
        result.deleted_recs_len_leaf);
    ninja_pt(print, "Delete-marked recs to page space ratio:   "
        "%02.05lf %%\n",
        static_cast<double>(
          result.deleted_recs_len_leaf) /
        g_page_physical_size * 100);

    result.innodb_internal_used_leaf =
      PAGE_NEW_SUPREMUM_END + result.headers_len_leaf +
      n_dir_slots * PAGE_DIR_SLOT_SIZE  + FIL_PAGE_DATA_END;
    ninja_pt(print, "\n");
    ninja_pt(print, "Total InnoDB internal space used:         %u B\n"
        "                                            "
        "[FIL HEADER     38 B]\n"
        "                                            "
        "[PAGE HEADER    36 B]\n"
        "                                            "
        "[FSEG HEADER    20 B]\n"
        "                                            "
        "[INFI + SUPRE   26 B]\n"
        "                                            "
        "[RECORD HEADERS %u B]*\n"
        "                                            "
        "[PAGE DIRECTORY %u B]\n"
        "                                            "
        "[FIL TRAILER    8 B]\n",
        result.innodb_internal_used_leaf,
        result.headers_len_leaf,
        n_dir_slots * PAGE_DIR_SLOT_SIZE);
    ninja_pt(print, "InnoDB internals to page space ratio:     "
        "%02.05lf %%\n",
        static_cast<double>(
          result.innodb_internal_used_leaf) /
        g_page_physical_size * 100);

    ninja_pt(print, "\n");
    result.free_leaf = garbage + UNIV_PAGE_SIZE - PAGE_DIR -
      n_dir_slots * PAGE_DIR_SLOT_SIZE - heap_top;
    ninja_pt(print, "Total free space:                         %u B\n",
        result.free_leaf);
    ninja_pt(print, "Free space ratio:                         "
        "%02.05lf %%\n",
        static_cast<double>(
          result.free_leaf) /
        g_page_physical_size * 100);
  } else {
    ninja_pt(print, "Total valid records count:               %u\n",
        result.n_recs_non_leaf);
    ninja_pt(print, "Total valid records size:                %u B\n"
        "                                           "
        "[Headers: %u B]\n"
        "                                           "
        "[Bodies : %u B)\n",
        result.headers_len_non_leaf + result.recs_len_non_leaf,
        result.headers_len_non_leaf, result.recs_len_non_leaf);
    ninja_pt(print, "Valid records to page space ratio:       "
        "%02.05lf %%\n",
        static_cast<double>(
          (result.headers_len_non_leaf +
           result.recs_len_non_leaf)) /
        g_page_physical_size * 100);

    ninja_pt(print, "\n");
    ninja_pt(print, "Total delete-marked records count:       %u\n",
        result.n_deleted_recs_non_leaf);
    ninja_pt(print, "Total delete-marked records size:        %u B\n",
        result.deleted_recs_len_non_leaf);
    ninja_pt(print, "Delete-marked recs to page space ratio:  "
        "%02.05lf %%\n",
        static_cast<double>(
          result.deleted_recs_len_non_leaf) /
        g_page_physical_size * 100);

    assert(result.n_contain_dropped_cols_recs_non_leaf == 0);
    assert(result.dropped_cols_len_non_leaf == 0);

    result.innodb_internal_used_non_leaf =
      PAGE_NEW_SUPREMUM_END + result.headers_len_non_leaf +
      n_dir_slots * PAGE_DIR_SLOT_SIZE  + FIL_PAGE_DATA_END;
    ninja_pt(print, "\n");
    ninja_pt(print, "Total innoDB internal space used:        %u B\n"
        "                                           "
        "[FIL HEADER     38 B]\n"
        "                                           "
        "[PAGE HEADER    36 B]\n"
        "                                           "
        "[FSEG HEADER    20 B]\n"
        "                                           "
        "[INFI + SUPRE   26 B]\n"
        "                                           "
        "[RECORD HEADERS %u B]*\n"
        "                                           "
        "[PAGE DIRECTORY %u B]\n"
        "                                           "
        "[FIL TRAILER    8 B]\n",
        result.innodb_internal_used_non_leaf,
        result.headers_len_non_leaf,
        n_dir_slots * PAGE_DIR_SLOT_SIZE);
    ninja_pt(print, "InnoDB internals to page space ratio:    "
        "%02.05lf %%\n",
        static_cast<double>(
          result.innodb_internal_used_non_leaf) /
        g_page_physical_size * 100);

    ninja_pt(print, "\n");
    result.free_non_leaf = garbage + UNIV_PAGE_SIZE - PAGE_DIR -
      n_dir_slots * PAGE_DIR_SLOT_SIZE - heap_top;
    ninja_pt(print, "Total free space:                        %u B\n",
        result.free_non_leaf);
    ninja_pt(print, "Free space ratio:                        "
        "%02.05lf %%\n",
        static_cast<double>(
          result.free_non_leaf) /
        g_page_physical_size * 100);
  }
  // aggregate the page result to the index result
  if (result_aggr != nullptr) {
    result_aggr->n_recs_non_leaf +=
      result.n_recs_non_leaf;
    result_aggr->n_recs_leaf +=
      result.n_recs_leaf;
    result_aggr->headers_len_non_leaf +=
      result.headers_len_non_leaf;
    result_aggr->headers_len_leaf +=
      result.headers_len_leaf;
    result_aggr->recs_len_non_leaf +=
      result.recs_len_non_leaf;
    result_aggr->recs_len_leaf +=
      result.recs_len_leaf;
    result_aggr->n_deleted_recs_non_leaf +=
      result.n_deleted_recs_non_leaf;
    result_aggr->n_deleted_recs_leaf +=
      result.n_deleted_recs_leaf;
    result_aggr->deleted_recs_len_non_leaf +=
      result.deleted_recs_len_non_leaf;
    result_aggr->deleted_recs_len_leaf +=
      result.deleted_recs_len_leaf;
    result_aggr->n_contain_dropped_cols_recs_non_leaf +=
      result.n_contain_dropped_cols_recs_non_leaf;
    result_aggr->n_contain_dropped_cols_recs_leaf +=
      result.n_contain_dropped_cols_recs_leaf;
    result_aggr->dropped_cols_len_non_leaf +=
      result.dropped_cols_len_non_leaf;
    result_aggr->dropped_cols_len_leaf +=
      result.dropped_cols_len_leaf;
    result_aggr->innodb_internal_used_non_leaf +=
      result.innodb_internal_used_non_leaf;
    result_aggr->innodb_internal_used_leaf +=
      result.innodb_internal_used_leaf;
    result_aggr->free_non_leaf +=
      result.free_non_leaf;
    result_aggr->free_leaf +=
      result.free_leaf;
  }

  return true;
}

bool ibdNinja::ToLeftmostLeaf(Index* index,
                              unsigned char* buf, uint32_t root,
                              std::vector<uint32_t>* leaf_pages_no) {
  if (!index->IsIndexParsingRecSupported()) {
    // ninja_warn("Skip getting leftmost pages");
    return false;
  }
  uint32_t bytes = ReadPage(root, buf);
  if (bytes != g_page_physical_size) {
    ninja_error("Failed to read page: %u, error: %d(%s)",
            root, errno, strerror(errno));
    return false;
  }
  uint32_t curr_page_no = root;
  leaf_pages_no->push_back(curr_page_no);

  uint32_t page_level = ReadFrom2B(buf + FIL_PAGE_DATA + PAGE_LEVEL);
  // uint32_t n_of_recs = ReadFrom2B(buf + FIL_PAGE_DATA + PAGE_N_RECS);

  unsigned char* current_rec = nullptr;
  while (page_level != 0) {
    current_rec = GetFirstUserRec(buf);
    if (current_rec == nullptr) {
      break;
    }
    Record record(current_rec, index);
    record.GetColumnOffsets();
    uint32_t child_page_no = record.GetChildPageNo();

    uint64_t curr_page_level = page_level;

    bytes = ReadPage(child_page_no, buf);
    if (bytes != g_page_physical_size) {
      ninja_error("Failed to read page: %u, error: %d(%s)",
              child_page_no, errno, strerror(errno));
      return false;
    }
    page_level = ReadFrom2B(buf + FIL_PAGE_DATA + PAGE_LEVEL);

    if (page_level != curr_page_level - 1) {
      break;
    }
    curr_page_no = child_page_no;
    leaf_pages_no->push_back(curr_page_no);
  }

  if (page_level != 0) {
    ninja_error("Failed to find leatmost page");
    return false;
  }
  return true;
}

bool ibdNinja::ParseIndex(uint32_t index_id) {
  auto iter = indexes_.find(index_id);
  if (iter == indexes_.end()) {
    ninja_error("Failed to parse the index. "
                "No index with ID %u was found", index_id);
    return false;
  }
  return ParseIndex(iter->second);
}

bool ibdNinja::ParseIndex(Index* index) {
  unsigned char buf_unalign[2 * UNIV_PAGE_SIZE_MAX];
  memset(buf_unalign, 0, 2 * UNIV_PAGE_SIZE_MAX);
  unsigned char* buf = static_cast<unsigned char*>(
                    ut_align(buf_unalign, g_page_physical_size));

  uint32_t page_no = index->ib_page();
  std::vector<uint32_t> left_pages_no;
  bool ret = ToLeftmostLeaf(index, buf, page_no, &left_pages_no);
  if (!ret) {
    return false;
  }
  uint32_t n_levels = left_pages_no.size();
  IndexAnalyzeResult index_result;
  fprintf(stdout, "\n");
  for (auto iter : left_pages_no) {
    fprintf(stdout, "Analyzing index %s at level %u...\n",
                    index->name().c_str(), --n_levels);
    index_result.n_level++;
    uint32_t current_page_no = iter;
    uint32_t next_page_no = FIL_NULL;
    do {
      ssize_t bytes = ReadPage(current_page_no, buf);
      if (bytes != g_page_physical_size) {
        ninja_error("Failed to read page: %u, error: %d(%s)",
            current_page_no, errno, strerror(errno));
        return false;
      }
      uint32_t page_level = ReadFrom2B(buf + PAGE_HEADER + PAGE_LEVEL);
      if (page_level > 0) {
        index_result.n_pages_non_leaf++;
      } else {
        index_result.n_pages_leaf++;
      }
      bool ret = ParsePage(current_page_no, &(index_result.recs_result),
                           false, true);
      if (!ret) {
        ninja_error("Error occurred while parsing page %u at level %u, "
                    "Skipping analysis for this level.",
                    current_page_no, n_levels);
        break;
      }
      next_page_no = ReadFrom4B(buf + FIL_PAGE_NEXT);
      current_page_no = next_page_no;
    } while (current_page_no != FIL_NULL);
  }
  fprintf(stdout, "=========================================="
                  "==========================================\n");
  fprintf(stdout, "|  INDEX ANALYSIS RESULT                   "
                  "                                         |\n");
  fprintf(stdout, "------------------------------------------"
                  "------------------------------------------\n");
  fprintf(stdout, "Index name:                                       %s\n",
                   index->name().c_str());
  fprintf(stdout, "Index id:                                         %u\n",
                   index->ib_id());
  fprintf(stdout, "Belongs to:                                       %s.%s\n",
                   index->table()->schema_ref().c_str(),
                   index->table()->name().c_str());
  fprintf(stdout, "Root page no:                                     %u\n",
                   index->ib_page());
  fprintf(stdout, "Num of fields(ALL):                               %u\n",
                   index->GetNFields());
  assert(left_pages_no.size() == index_result.n_level);
  fprintf(stdout, "Num of levels:                                    %u\n",
                   index_result.n_level);
  fprintf(stdout, "Num of pages:                                     %u\n"
                  "                                                  "
                  "  [Non leaf pages: %u]\n"
                  "                                                  "
                  "  [Leaf pages:     %u]\n",
                   index_result.n_pages_non_leaf + index_result.n_pages_leaf,
                   index_result.n_pages_non_leaf, index_result.n_pages_leaf);
  if (index_result.n_level > 1) {
    uint32_t total_pages_size = index_result.n_pages_non_leaf *
                                g_page_physical_size;
    // Print non-leaf pages statistic
    fprintf(stdout, "\n--------NON-LEAF-LEVELS--------\n");
    fprintf(stdout, "Total pages count:                                "
                    "%u\n",
                     index_result.n_pages_non_leaf);
    fprintf(stdout, "Total pages size:                                 "
                    "%u B\n",
                     total_pages_size);

    fprintf(stdout, "\n");
    fprintf(stdout, "Total valid records count:                        "
                    "%u\n",
                     index_result.recs_result.n_recs_non_leaf);
    fprintf(stdout, "Total valid records size:                         "
                    "%u B\n"
                    "                                                  "
                    "  [Headers: %u B]\n"
                    "                                                  "
                    "  [Bodies:  %u B]\n",
                     index_result.recs_result.headers_len_non_leaf +
                     index_result.recs_result.recs_len_non_leaf,
                     index_result.recs_result.headers_len_non_leaf,
                     index_result.recs_result.recs_len_non_leaf);
    fprintf(stdout, "Valid records to non-leaf pages space ratio:      "
        "%02.05lf %%\n",
        static_cast<double>(
          (index_result.recs_result.headers_len_non_leaf +
           index_result.recs_result.recs_len_non_leaf)) /
        total_pages_size * 100);

    fprintf(stdout, "\n");
    fprintf(stdout, "Total delete-marked records count:                "
                    "%u\n",
                     index_result.recs_result.n_deleted_recs_non_leaf);
    fprintf(stdout, "Total delete-marked records size:                 "
                     "%u B\n",
                     index_result.recs_result.deleted_recs_len_non_leaf);
    fprintf(stdout, "Delete-marked recs to non-leaf pages space ratio: "
        "%02.05lf %%\n",
        static_cast<double>(
          index_result.recs_result.deleted_recs_len_non_leaf) /
        total_pages_size * 100);

    assert(index_result.recs_result.n_contain_dropped_cols_recs_non_leaf == 0);
    assert(index_result.recs_result.dropped_cols_len_non_leaf == 0);

    fprintf(stdout, "\n");
    fprintf(stdout, "Total Innodb internal space used:                 "
                    "%u B\n",
                    index_result.recs_result.innodb_internal_used_non_leaf);
    fprintf(stdout, "InnoDB internals to non-leaf pages space ratio:   "
        "%02.05lf %%\n",
        static_cast<double>(
          index_result.recs_result.innodb_internal_used_non_leaf) /
        total_pages_size * 100);

    fprintf(stdout, "\n");
    fprintf(stdout, "Total free space:                                 "
                    "%u B\n",
                    index_result.recs_result.free_non_leaf);
    fprintf(stdout, "Free space ratio:                                 "
                    "%02.05lf %%\n",
                     static_cast<double>(
                      index_result.recs_result.free_non_leaf) /
                      total_pages_size * 100);
  }
  uint32_t total_pages_size = index_result.n_pages_leaf * g_page_physical_size;
  fprintf(stdout, "\n--------LEAF-LEVEL---------------\n");
  fprintf(stdout, "Total pages count:                                "
                  "%u\n",
                   index_result.n_pages_leaf);
  fprintf(stdout, "Total pages size:                                 "
                  "%u B\n",
                   total_pages_size);
  fprintf(stdout, "\n");
  fprintf(stdout, "Total valid records count:                        "
                  "%u\n",
                   index_result.recs_result.n_recs_leaf);
  fprintf(stdout, "Total valid records size:                         "
                  "%u B\n"
                  "                                                  "
                  "  [Headers: %u B]\n"
                  "                                                  "
                  "  [Bodies:  %u B]\n",
                   index_result.recs_result.headers_len_leaf +
                   index_result.recs_result.recs_len_leaf,
                   index_result.recs_result.headers_len_leaf,
                   index_result.recs_result.recs_len_leaf);
  fprintf(stdout, "Valid records to leaf pages space ratio:          "
                  "%02.05lf %%\n",
                   static_cast<double>(
                   (index_result.recs_result.headers_len_leaf +
                    index_result.recs_result.recs_len_leaf)) /
                    total_pages_size * 100);

  fprintf(stdout, "\n");
  fprintf(stdout, "Total records with instant dropped columns count: "
                  "%u\n",
                   index_result.recs_result.n_contain_dropped_cols_recs_leaf);
  fprintf(stdout, "Total instant dropped columns size:               "
                  "%u B\n",
                   index_result.recs_result.dropped_cols_len_leaf);
  fprintf(stdout, "Dropped columns to leaf pages space ratio:        "
                  "%02.05lf %%\n",
                   static_cast<double>(
                    index_result.recs_result.dropped_cols_len_leaf) /
                    total_pages_size * 100);

  fprintf(stdout, "\n");
  fprintf(stdout, "Total delete-marked records count:                "
                  "%u\n",
                   index_result.recs_result.n_deleted_recs_leaf);
  fprintf(stdout, "Total delete-marked records size:                 "
                   "%u B\n",
                   index_result.recs_result.deleted_recs_len_leaf);
  fprintf(stdout, "Delete-marked records to leaf pages space ratio:  "
                  "%02.05lf %%\n",
                   static_cast<double>(
                    index_result.recs_result.deleted_recs_len_leaf) /
                    total_pages_size * 100);

  fprintf(stdout, "\n");
  fprintf(stdout, "Total Innodb internal space used:                 "
                  "%u B\n",
                   index_result.recs_result.innodb_internal_used_leaf);
  fprintf(stdout, "InnoDB internal space to leaf pages space ratio:  "
                  "%02.05lf %%\n",
                   static_cast<double>(
                    index_result.recs_result.innodb_internal_used_leaf) /
                    total_pages_size * 100);

  fprintf(stdout, "\n");
  fprintf(stdout, "Total free space:                                 "
                  "%u B\n",
                   index_result.recs_result.free_leaf);
  fprintf(stdout, "Free space ratio:                                 "
                  "%02.05lf %%\n",
                   static_cast<double>(
                    index_result.recs_result.free_leaf) /
                    total_pages_size * 100);

  return ret;
}

void ibdNinja::ShowTables(bool only_supported) {
  if (!only_supported) {
    fprintf(stdout, "Listing all tables and indexes "
                    "in the specified ibd file:\n");
    for (auto table : all_tables_) {
      fprintf(stdout, "---------------------------------------\n");
      fprintf(stdout, "[Table] name: %s.%s\n",
              table->schema_ref().c_str(),
              table->name().c_str());
      for (auto index : table->indexes()) {
        fprintf(stdout, "        [Index] name: %s\n",
                index->name().c_str());
      }
    }
  } else {
    fprintf(stdout, "Listing all *supported* tables and indexes "
                    "in the specified ibd file:\n");
    for (auto table : tables_) {
      fprintf(stdout, "---------------------------------------\n");
      fprintf(stdout, "[Table] id: %-7" PRIu64 " name: %s.%s\n", table.first,
          table.second->schema_ref().c_str(),
          table.second->name().c_str());
      for (auto index : table.second->indexes()) {
        if (index->IsIndexSupported() &&
            indexes_.find(index->ib_id()) != indexes_.end()) {
          fprintf(stdout, "        [Index] id: %-7u, "
              "root page no: %-7u, name: %s\n",
              index->ib_id(), index->ib_page(),
              index->name().c_str());
        }
      }
    }
  }
}

void ibdNinja::ShowLeftmostPages(uint32_t index_id) {
  auto iter = indexes_.find(index_id);
  if (iter == indexes_.end()) {
    ninja_error("Failed to parse the index. "
                "No index with ID %u was found", index_id);
    return;
  }
  Index* index = iter->second;
  unsigned char buf_unalign[2 * UNIV_PAGE_SIZE_MAX];
  memset(buf_unalign, 0, 2 * UNIV_PAGE_SIZE_MAX);
  unsigned char* buf = static_cast<unsigned char*>(
                    ut_align(buf_unalign, g_page_physical_size));

  uint32_t page_no = index->ib_page();
  std::vector<uint32_t> left_pages_no;
  bool ret = ToLeftmostLeaf(index, buf, page_no, &left_pages_no);
  if (!ret) {
    return;
  }

  uint32_t n_levels = left_pages_no.size();
  uint32_t curr_level = n_levels - 1;
  fprintf(stdout, "---------------------------------------\n");
  fprintf(stdout, "Table name: %s.%s\n",
                   index->table()->schema_ref().c_str(),
                   index->table()->name().c_str());
  fprintf(stdout, "Index name: %s\n",
                   index->name().c_str());
  for (auto iter : left_pages_no) {
    fprintf(stdout, "  Level %u: page %u\n",
        curr_level, iter);
    curr_level--;
  }
}

bool ibdNinja::ParseTable(uint32_t table_id) {
  auto iter = tables_.find(table_id);
  if (iter == tables_.end()) {
    ninja_error("Failed to parse the table. "
                "No table with ID %u was found", table_id);
    return false;
  }
  assert(iter->second != nullptr);
  fprintf(stdout, "=========================================="
                  "==========================================\n");
  fprintf(stdout, "|  TABLE ANALYSIS RESULT                   "
                  "                                         |\n");
  fprintf(stdout, "------------------------------------------"
                  "------------------------------------------\n");
  fprintf(stdout, "Table name:        %s.%s\n",
                   iter->second->schema_ref().c_str(),
                   iter->second->name().c_str());
  fprintf(stdout, "Table id:          %u\n",
                   iter->second->ib_id());
  fprintf(stdout, "Number of indexes: %lu\n",
                   iter->second->indexes().size());
  fprintf(stdout, "Analyze each index:\n");
  for (auto index : iter->second->indexes()) {
    if (index->IsIndexSupported()) {
      ParseIndex(index);
    }
  }
  return true;
}
}  // namespace ibd_ninja
