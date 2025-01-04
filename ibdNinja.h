/*
 * Copyright (c) [2025] [Zhao Song]
 */
#ifndef IBDNINJA_H_
#define IBDNINJA_H_

#include "ibdUtils.h"

#include <rapidjson/document.h>

#include <iostream>
#include <optional>
#include <vector>
#include <string>
#include <limits>
#include <set>
#include <map>


namespace ibd_ninja {

class Properties {
 public:
  Properties() = default;
  explicit Properties(const std::set<std::string>& keys) : keys_(keys) {
  }
  bool InsertValues(const std::string& opt_string);
  void DebugDump(int space = 0) {
    std::string space_str(space, ' ');

    std::cout << space_str << "[" << std::endl;
    std::cout << space_str << "Dump Properties:" << std::endl;

    for (const auto& iter : kvs_) {
      std::cout << space_str << "  " << iter.first << ": "
                << iter.second << std::endl;
    }

    std::cout << space_str << "]" << std::endl;
  }

  template <typename T>
  bool Get(const std::string& key, T* value) const;
  bool Exists(const std::string& key) const;

 private:
  bool ValidKey(const std::string& key) const;
  std::set<std::string> keys_;
  std::map<std::string, std::string> kvs_;
};

class IndexColumn;
class Column {
 public:
  static Column* CreateColumn(const rapidjson::Value& dd_col_obj);
  // Only used in creating SE system column
  Column(std::string name, uint32_t ind) :
         dd_name_(name), dd_is_nullable_(false),
         dd_is_virtual_(false), ib_ind_(ind),
         ib_mtype_(DATA_SYS), ib_is_visible_(false),
         ib_version_added_(0), ib_version_dropped_(0),
         ib_phy_pos_(UINT32_UNDEFINED),
         se_explicit_(true),
         index_column_(nullptr) {
  }
  // Only used in creating FTS_DOC_ID column
  Column(std::string name, uint32_t ind, bool fts) :
         dd_name_(name), dd_is_nullable_(false),
         dd_is_virtual_(false), ib_ind_(ind),
         ib_mtype_(DATA_INT), ib_is_visible_(false),
         ib_version_added_(UINT8_UNDEFINED),
         ib_version_dropped_(UINT8_UNDEFINED),
         ib_phy_pos_(UINT32_UNDEFINED),
         se_explicit_(true),
         index_column_(nullptr) {
           assert(fts);
  }
  void DebugDump(int space = 0) {
    std::string space_str(space, ' ');
    std::cout << space_str << "[" << std::endl;
    std::cout << space_str << "Dump Column:" << std::endl;
    std::cout << space_str << "  " << "name: "
              << dd_name_ << std::endl
              << space_str << "  " << "type: "
              << dd_type_ << std::endl
              << space_str << "  " << "is_nullable: "
              << dd_is_nullable_ << std::endl
              << space_str << "  " << "is_zerofill: "
              << dd_is_zerofill_ << std::endl
              << space_str << "  " << "is_unsigned: "
              << dd_is_unsigned_ << std::endl
              << space_str << "  " << "is_auto_increment: "
              << dd_is_auto_increment_ << std::endl
              << space_str << "  " << "is_virtual: "
              << dd_is_virtual_ << std::endl
              << space_str << "  " << "hidden: "
              << dd_hidden_ << std::endl
              << space_str << "  " << "ordinal_position: "
              << dd_ordinal_position_ << std::endl
              << space_str << "  " << "char_length: "
              << dd_char_length_ << std::endl
              << space_str << "  " << "numeric_precision: "
              << dd_numeric_precision_ << std::endl
              << space_str << "  " << "numeric_scale: "
              << dd_numeric_scale_ << std::endl
              << space_str << "  " << "numeric_scale_null: "
              << dd_numeric_scale_null_ << std::endl
              << space_str << "  " << "datetime_precision: "
              << dd_datetime_precision_ << std::endl
              << space_str << "  " << "datetime_precision_null: "
              << dd_datetime_precision_null_ << std::endl
              << space_str << "  " << "has_no_default: "
              << dd_has_no_default_ << std::endl
              << space_str << "  " << "default_value_null: "
              << dd_default_value_null_ << std::endl
              << space_str << "  " << "srs_id_null: "
              << dd_srs_id_null_ << std::endl
              << space_str << "  " << "srs_id: "
              << (dd_srs_id_.has_value() ? dd_srs_id_.value() : 0) << std::endl
              << space_str << "  " << "default_value: "
              << dd_default_value_ << std::endl
              << space_str << "  " << "default_value_utf8_null: "
              << dd_default_value_utf8_null_ << std::endl
              << space_str << "  " << "default_value_utf8"
              << dd_default_value_utf8_ << std::endl
              << space_str << "  " << "default_option: "
              << dd_default_option_ << std::endl
              << space_str << "  " << "update_option: "
              << dd_update_option_ << std::endl
              << space_str << "  " << "comment: "
              << dd_comment_ << std::endl
              << space_str << "  " << "generation_expression: "
              << dd_generation_expression_ << std::endl
              << space_str << "  " << "generation_expression_utf8: "
              << dd_generation_expression_utf8_ << std::endl;
    std::cout << space_str << "  " << "options: " << std::endl;
    dd_options_.DebugDump(space + 4);
    std::cout << space_str << "  " << "se_private_data: " << std::endl;
    dd_se_private_data_.DebugDump(space + 4);

    std::cout << space_str << "  " << "engine_attribute: "
              << dd_engine_attribute_ << std::endl
              << space_str << "  " << "secondary_engine_attribute: "
              << dd_secondary_engine_attribute_ << std::endl
              << space_str << "  " << "column_key: "
              << dd_column_key_ << std::endl
              << space_str << "  " << "column_type_utf8: "
              << dd_column_type_utf8_ << std::endl
              // TODO(Zhao):
              // dd_elements_
              << space_str << "  " << "collation_id: "
              << dd_collation_id_ << std::endl
              << space_str << "  " << "is_explicit_collation: "
              << dd_is_explicit_collation_ << std::endl;
    std::cout << space_str << "]" << std::endl;
  }
  enum enum_column_types {
    // 31 in total
    DECIMAL = 1,
    TINY,
    SHORT,
    LONG,
    FLOAT,
    DOUBLE,
    TYPE_NULL,
    TIMESTAMP,
    LONGLONG,
    INT24,
    DATE,
    TIME,
    DATETIME,
    YEAR,
    NEWDATE,
    VARCHAR,
    BIT,
    TIMESTAMP2,
    DATETIME2,
    TIME2,
    NEWDECIMAL,
    ENUM,
    SET,
    TINY_BLOB,
    MEDIUM_BLOB,
    LONG_BLOB,
    BLOB,
    VAR_STRING,
    STRING,
    GEOMETRY,
    JSON
  };
  enum enum_column_key {
    CK_NONE = 1,
    CK_PRIMARY,
    CK_UNIQUE,
    CK_MULTIPLE
  };
  enum enum_hidden_type {
    HT_VISIBLE = 1,
    HT_HIDDEN_SE = 2,
    HT_HIDDEN_SQL = 3,
    HT_HIDDEN_USER = 4
  };
  enum enum_field_types {
    /*
     * 34 in total, the below 3 are different with enum_column_fields
     * MYSQL_TYPE_TYPED_ARRAY
     * MYSQL_TYPE_INVALID
     * MYSQL_TYPE_BOOL
     */
    MYSQL_TYPE_DECIMAL,
    MYSQL_TYPE_TINY,
    MYSQL_TYPE_SHORT,
    MYSQL_TYPE_LONG,
    MYSQL_TYPE_FLOAT,
    MYSQL_TYPE_DOUBLE,
    MYSQL_TYPE_NULL,
    MYSQL_TYPE_TIMESTAMP,
    MYSQL_TYPE_LONGLONG,
    MYSQL_TYPE_INT24,
    MYSQL_TYPE_DATE,
    MYSQL_TYPE_TIME,
    MYSQL_TYPE_DATETIME,
    MYSQL_TYPE_YEAR,
    MYSQL_TYPE_NEWDATE, /**< Internal to MySQL. Not used in protocol */
    MYSQL_TYPE_VARCHAR,
    MYSQL_TYPE_BIT,
    MYSQL_TYPE_TIMESTAMP2,
    MYSQL_TYPE_DATETIME2,   /**< Internal to MySQL. Not used in protocol */
    MYSQL_TYPE_TIME2,       /**< Internal to MySQL. Not used in protocol */
    MYSQL_TYPE_TYPED_ARRAY, /**< Used for replication only */
    MYSQL_TYPE_INVALID = 243,
    MYSQL_TYPE_BOOL = 244, /**< Currently just a placeholder */
    MYSQL_TYPE_JSON = 245,
    MYSQL_TYPE_NEWDECIMAL = 246,
    MYSQL_TYPE_ENUM = 247,
    MYSQL_TYPE_SET = 248,
    MYSQL_TYPE_TINY_BLOB = 249,
    MYSQL_TYPE_MEDIUM_BLOB = 250,
    MYSQL_TYPE_LONG_BLOB = 251,
    MYSQL_TYPE_BLOB = 252,
    MYSQL_TYPE_VAR_STRING = 253,
    MYSQL_TYPE_STRING = 254,
    MYSQL_TYPE_GEOMETRY = 255
  };
  static const std::set<std::string> default_valid_option_keys;

  const std::string& name() const {
    return dd_name_;
  }
  enum_column_types type() const {
    return dd_type_;
  }
  // Only used in setting SE DB_TRX_ID column type
  void set_type(enum_column_types type) {
    dd_type_ = type;
  }
  bool is_nullable() const {
    return dd_is_nullable_;
  }
  bool is_virtual() const {
    return dd_is_virtual_;
  }
  enum_hidden_type hidden() const {
    return dd_hidden_;
  }
  bool IsSeHidden() const {
    return dd_hidden_ == enum_hidden_type::HT_HIDDEN_SE;
  }
  const Properties& options() const {
    return dd_options_;
  }
  const Properties& se_private_data() const {
    return dd_se_private_data_;
  }

  /*------TABLE SHARE------*/
  static enum_field_types DDType2FieldType(enum_column_types);
  enum_field_types FieldType() const;
  bool IsBinary() const;
  uint32_t PackLength() const;
  static uint32_t VarcharLenBytes(uint32_t char_length) {
    return ((char_length) < 256 ? 1 : 2);
  }
  uint32_t VarcharLenBytes() const {
    return ((dd_char_length_) < 256 ? 1 : 2);
  }

  std::string FieldTypeString();

  /*------SE------*/
  bool IsSystemColumn() const {
    return (dd_name_ == "DB_ROW_ID" || dd_name_ == "DB_TRX_ID" ||
            dd_name_ == "DB_ROLL_PTR");
  }
  bool IsColumnAdded() const;
  uint32_t GetVersionAdded() const;
  bool IsInstantAdded() const;
  bool IsColumnDropped() const;
  uint32_t GetVersionDropped() const;
  bool IsInstantDropped() const;
  uint32_t FieldType2SeType() const;
  std::string SeTypeString();

  uint32_t ib_ind() {
    return ib_ind_;
  }
  void set_ib_ind(uint32_t ind) {
    ib_ind_ = ind;
  }
  std::string dd_column_type_utf8() {
    return dd_column_type_utf8_;
  }
  uint32_t ib_mtype() {
    return ib_mtype_;
  }
  void set_ib_mtype(uint32_t m_type) {
    ib_mtype_ = m_type;
  }
  uint32_t ib_is_visible() {
    return ib_is_visible_;
  }
  void set_ib_is_visible(bool ib_is_visible) {
    ib_is_visible_ = ib_is_visible;
  }
  uint32_t ib_version_added() {
    return ib_version_added_;
  }
  void set_ib_version_added(uint32_t version_added) {
    ib_version_added_ = version_added;
  }
  uint32_t ib_version_dropped() {
    return ib_version_dropped_;
  }
  void set_ib_version_dropped(uint32_t version_dropped) {
    ib_version_dropped_ = version_dropped;
  }
  uint32_t ib_phy_pos() {
    return ib_phy_pos_;
  }
  void set_ib_phy_pos(uint32_t phy_pos) {
    ib_phy_pos_ = phy_pos;
  }
  uint32_t ib_col_len() {
    return ib_col_len_;
  }
  void set_ib_col_len(uint32_t col_len) {
    ib_col_len_ = col_len;
  }
  IndexColumn* index_column() {
    return index_column_;
  }
  void set_index_column(IndexColumn* index_column) {
    index_column_ = index_column;
  }
  void set_ib_instant_default(bool instant_default) {
    ib_instant_default_ = instant_default;
  }
  bool ib_instant_default() {
    return ib_instant_default_;
  }
  bool se_explicit() {
    return se_explicit_;
  }

  uint32_t GetFixedSize();
  bool IsDroppedInOrBefore(uint8_t version) const;
  bool IsAddedAfter(uint8_t version) const;
  bool IsBigCol() const;

 private:
  Column() : dd_options_(default_valid_option_keys),
             dd_se_private_data_(),
             se_explicit_(false), index_column_(nullptr) {
  }
  bool Init(const rapidjson::Value& dd_col_obj);
  std::string dd_name_;
  enum_column_types dd_type_;
  bool dd_is_nullable_;
  bool dd_is_zerofill_;
  bool dd_is_unsigned_;
  bool dd_is_auto_increment_;
  bool dd_is_virtual_;
  enum_hidden_type dd_hidden_;
  uint32_t dd_ordinal_position_;
  uint32_t dd_char_length_;
  uint32_t dd_numeric_precision_;
  uint32_t dd_numeric_scale_;
  bool dd_numeric_scale_null_;
  uint32_t dd_datetime_precision_;
  uint32_t dd_datetime_precision_null_;
  bool dd_has_no_default_;
  bool dd_default_value_null_;
  bool dd_srs_id_null_;
  std::optional<std::uint32_t> dd_srs_id_;
  std::string dd_default_value_;
  bool dd_default_value_utf8_null_;
  std::string dd_default_value_utf8_;
  std::string dd_default_option_;
  std::string dd_update_option_;
  std::string dd_comment_;
  std::string dd_generation_expression_;
  std::string dd_generation_expression_utf8_;
  Properties dd_options_;
  Properties dd_se_private_data_;
  std::string dd_engine_attribute_;
  std::string dd_secondary_engine_attribute_;
  enum_column_key dd_column_key_;
  std::string dd_column_type_utf8_;
  // TODO(Zhao):
  // Column_type_element_collection dd_elements_;
  uint64_t dd_elements_size_tmp_;
  uint64_t dd_collation_id_;
  bool dd_is_explicit_collation_;

  /*------SE------*/
  uint32_t ib_ind_;
  uint32_t ib_mtype_;
  bool ib_is_visible_;
  uint32_t ib_version_added_;
  uint32_t ib_version_dropped_;
  uint32_t ib_phy_pos_;
  uint32_t ib_col_len_;
  bool ib_instant_default_;

  bool se_explicit_;
  IndexColumn* index_column_;
};

class IndexColumn {
 public:
  static IndexColumn* CreateIndexColumn(
                         const rapidjson::Value& dd_index_col_obj,
                         const std::vector<Column*>& columns);
  // Used only when creating index columns for a dropped column.
  static IndexColumn* CreateIndexDroppedColumn(Column* dropped_col);
  // Used only when creating a FTS_DOC_ID index column.
  static IndexColumn* CreateIndexFTSDocIdColumn(Column* doc_id_col);
  void DebugDump(int space = 0) {
    std::string space_str(space, ' ');
    std::cout << space_str << "[" << std::endl;

    std::cout << space_str << "Dump IndexColumn:" << std::endl;
    std::cout << space_str << "  " << "ordinal_position: "
              << dd_ordinal_position_ << std::endl
              << space_str << "  " << "length: "
              << dd_length_ << std::endl
              << space_str << "  " << "order: "
              << dd_order_ << std::endl
              << space_str << "  " << "hidden: "
              << dd_hidden_ << std::endl;

    std::cout << space_str << "]" << std::endl;
  }
  enum enum_index_element_order {
    ORDER_UNDEF = 1,
    ORDER_ASC,
    ORDER_DESC
  };

  Column* column() const {
    return column_;
  }
  uint32_t length() const {
    return dd_length_;
  }
  bool hidden() const {
    return dd_hidden_;
  }
  uint32_t ib_fixed_len() {
    return ib_fixed_len_;
  }
  void set_ib_fixed_len(uint32_t fixed_len) {
    ib_fixed_len_ = fixed_len;
  }
  void set_column(Column* column) {
    column_ = column;
  }
  bool se_explicit() {
    return se_explicit_;
  }

 private:
  explicit IndexColumn(bool se_explicit) : se_explicit_(se_explicit),
    column_(nullptr) {
  }
  bool Init(const rapidjson::Value& dd_index_col_obj,
            const std::vector<Column*>& columns);
  uint32_t dd_ordinal_position_;
  uint32_t dd_length_;
  enum_index_element_order dd_order_;
  bool dd_hidden_;
  uint32_t dd_column_opx_;

  /*------SE------*/
  uint32_t ib_fixed_len_;

  bool se_explicit_;

  Column* column_;
};

constexpr uint32_t DICT_CLUSTERED = 1;
constexpr uint32_t DICT_UNIQUE = 2;
constexpr uint32_t DICT_FTS = 32;
constexpr uint32_t DICT_SPATIAL = 64;
const uint8_t MAX_ROW_VERSION = 64;
class Table;
class Index {
 public:
  static Index* CreateIndex(const rapidjson::Value& dd_index_obj,
                            const std::vector<Column*>& columns,
                            Table* table);
  ~Index() {
    if (IsClustered()) {
      for (auto* iter : ib_fields_) {
        if (iter->se_explicit()) {
          delete iter;
        }
      }
    }
    for (auto* iter : dd_elements_) {
      delete iter;
    }
  }
  void DebugDump(int space = 0) {
    std::string space_str(space, ' ');
    std::cout << space_str << "[" << std::endl;
    std::cout << space_str << "Dump Index:" << std::endl;
    std::cout << space_str << "  " << "name: "
              << dd_name_ << std::endl
              << space_str << "  " << "hidden: "
              << dd_hidden_ << std::endl
              << space_str << "  " << "is_generated: "
              << dd_is_generated_ << std::endl
              << space_str << "  " << "ordinal_position: "
              << dd_ordinal_position_ << std::endl
              << space_str << "  " << "comment: "
              << dd_comment_ << std::endl
              << space_str << "  " << "options: " << std::endl;
    dd_options_.DebugDump(space + 4);
    std::cout << space_str << "  " << "se_private_data: " << std::endl;
    dd_se_private_data_.DebugDump(space + 4);

    std::cout << space_str << "  " << "type: "
              << dd_type_ << std::endl
              << space_str << "  " << "algorithm: "
              << dd_algorithm_ << std::endl
              << space_str << "  " << "is_algorithm_explicit: "
              << dd_is_algorithm_explicit_ << std::endl
              << space_str << "  " << "is_visible: "
              << dd_is_visible_ << std::endl
              << space_str << "  " << "engine: "
              << dd_engine_ << std::endl
              << space_str << "  " << "engine_attribute: "
              << dd_engine_attribute_ << std::endl
              << space_str << "  " << "secondary_engine_attribute: "
              << dd_secondary_engine_attribute_ << std::endl
              << space_str << "  " << "tablespace_ref: "
              << dd_tablespace_ref_ << std::endl;

    std:: cout << space_str << "  " << "elements: " << std::endl;
    for (const auto& iter : dd_elements_) {
      iter->DebugDump(space + 4);
    }

    std::cout << space_str << "]" << std::endl;
  }
  enum enum_index_type {
    IT_PRIMARY = 1,
    IT_UNIQUE,
    IT_MULTIPLE,
    IT_FULLTEXT,
    IT_SPATIAL
  };
  enum enum_index_algorithm {
    IA_SE_SPECIFIC = 1,
    IA_BTREE,
    IA_RTREE,
    IA_HASH,
    IA_FULLTEXT
  };

  static const std::set<std::string> default_valid_option_keys;

  const std::string& name() const {
    return dd_name_;
  }
  enum_index_type type() const {
    return dd_type_;
  }
  const Properties& se_private_data() const {
    return dd_se_private_data_;
  }

  /* ------TABLE SHARE------ */
  bool FillIndex(uint32_t ind);
  uint32_t s_user_defined_key_parts() {
    return s_user_defined_key_parts_;
  }
  uint32_t s_key_length() {
    return s_key_length_;
  }
  uint32_t s_flags() {
    return s_flags_;
  }

  /* ------SE------ */
  bool FillSeIndex(uint32_t ind);
  bool IndexAddCol(Column* column, uint32_t prefix_len);
  bool IsClustered() {
    return (ib_type_ & DICT_CLUSTERED);
  }
  uint32_t ib_id() {
    return ib_id_;
  }
  // root page
  uint32_t ib_page() {
    return ib_page_;
  }
  uint32_t ib_n_fields() {
    return ib_n_fields_;
  }
  uint32_t ib_n_uniq() {
    return ib_n_uniq_;
  }
  uint32_t ib_type() {
    return ib_type_;
  }
  uint32_t ib_n_def() {
    return ib_n_def_;
  }
  uint32_t ib_n_nullable() {
    return ib_n_nullable_;
  }
  std::vector<IndexColumn*>* ib_fields() {
    return &ib_fields_;
  }
  bool IsIndexUnique() {
    return (ib_type_ & DICT_UNIQUE);
  }

  std::vector<uint16_t>* ib_fields_array() {
    return &ib_fields_array_;
  }
  uint32_t* ib_nullables() {
    return ib_nullables_;
  }
  bool ib_row_versions() {
    return ib_row_versions_;
  }
  bool ib_instant_cols() {
    return ib_instant_cols_;
  }
  uint32_t ib_n_instant_nullable() {
    return ib_n_instant_nullable_;
  }

  uint32_t GetNFields() const;
  uint32_t ib_n_total_fields() {
    return ib_n_total_fields_;
  }
  Table* table() const {
    return table_;
  }
  uint32_t GetNOriginalFields();
  uint32_t GetNNullableBefore(uint32_t nth);
  uint32_t CalculateNInstantNullable(uint32_t n_fields);
  bool HasInstantColsOrRowVersions();
  uint32_t GetNullableInVersion(uint8_t version);
  uint16_t GetNullableBeforeInstantAddDrop();
  uint16_t GetNUniqueInTree();
  uint16_t GetNUniqueInTreeNonleaf();
  IndexColumn* GetPhysicalField(size_t pos);

  bool IsIndexSupported();
  std::string UnsupportedReason();
  bool IsIndexParsingPageSupported();
  bool IsIndexParsingRecSupported();

 private:
  explicit Index(Table* table) :
            dd_options_(default_valid_option_keys),
            dd_se_private_data_(),
            unsupported_reason_(0),
            ib_type_(0),
            table_(table) {
  }
  bool Init(const rapidjson::Value& dd_index_obj,
            const std::vector<Column*>& columns);
  std::string dd_name_;
  bool dd_hidden_;
  bool dd_is_generated_;
  uint32_t dd_ordinal_position_;
  std::string dd_comment_;
  Properties dd_options_;
  Properties dd_se_private_data_;
  enum_index_type dd_type_;
  enum_index_algorithm dd_algorithm_;
  bool dd_is_algorithm_explicit_;
  bool dd_is_visible_;
  std::string dd_engine_;
  std::string dd_engine_attribute_;
  std::string dd_secondary_engine_attribute_;
  std::vector<IndexColumn*> dd_elements_;
  std::string dd_tablespace_ref_;

  /* ------TABLE SHARE------ */
  uint32_t s_user_defined_key_parts_;
  uint32_t s_key_length_;
  uint32_t s_flags_;

  /* ------SE------ */
  void PreCheck();
  uint32_t unsupported_reason_;
  uint32_t ib_id_;
  uint32_t ib_page_;  // root page
  uint32_t ib_n_fields_;
  uint32_t ib_n_uniq_;
  uint32_t ib_type_;
  uint32_t ib_n_def_;
  uint32_t ib_n_nullable_;
  uint32_t ib_n_user_defined_cols_;
  std::vector<IndexColumn*> ib_fields_;
  std::vector<uint16_t> ib_fields_array_;
  uint32_t ib_nullables_[MAX_ROW_VERSION + 1] = {0};
  bool ib_row_versions_;
  bool ib_instant_cols_;
  uint32_t ib_n_instant_nullable_;
  uint32_t ib_n_total_fields_;
  Table* table_;
};

class Table {
 public:
  static Table* CreateTable(const rapidjson::Value& dd_obj,
                            unsigned char* sdi_data);
  ~Table() {
    for (auto iter : indexes_) {
      delete iter;
    }
    for (auto iter : ib_cols_) {
      if (iter->se_explicit()) {
        delete iter;
      }
    }
    for (auto iter : columns_) {
      delete iter;
    }
    delete[] sdi_data_;
  }
  void DebugDump() {
    std::cout << "Dump Table:" << std::endl
              << "  name: " << dd_name_ << std::endl
              << "  mysql_version_id: " << dd_mysql_version_id_ << std::endl
              << "  created: " << dd_created_ << std::endl
              << "  last_altered: " << dd_last_altered_ << std::endl
              << "  hidden: " << dd_hidden_ << std::endl
              << "  options: " << std::endl;
    dd_options_.DebugDump(4);
    std::cout << "  schema_ref: "
              << dd_schema_ref_ << std::endl
              << "  se_private_id: "
              << dd_se_private_id_ << std::endl
              << "  engine: "
              << dd_engine_ << std::endl
              << "  comment: "
              << dd_comment_ << std::endl
              << "  last_checked_for_upgrade_version_id: "
              << dd_last_checked_for_upgrade_version_id_ << std::endl
              << "  se_private_data: " << std::endl;
    dd_se_private_data_.DebugDump(4);

    std::cout << "  engine_attribute: "
              << dd_engine_attribute_ << std::endl
              << "  secondary_engine_attribute: "
              << dd_secondary_engine_attribute_ << std::endl
              << "  row_format: "
              << dd_row_format_ << std::endl
              << "  partition_type: "
              << dd_partition_type_ << std::endl
              << "  partition_expression: "
              << dd_partition_expression_ << std::endl
              << "  partition_expression_utf8: "
              << dd_partition_expression_utf8_ << std::endl
              << "  default_partitioning: "
              << dd_default_partitioning_ << std::endl
              << "  subpartition_type: "
              << dd_subpartition_type_ << std::endl
              << "  subpartition_expression: "
              << dd_subpartition_expression_ << std::endl
              << "  subpartition_expression_utf8: "
              << dd_subpartition_expression_utf8_ << std::endl
              << "  default_subpartitioning: "
              << dd_default_subpartitioning_ << std::endl
              << "  collation_id: "
              << dd_collation_id_ << std::endl;

    std::cout << "  columns: " << std::endl;
    for (const auto& iter : columns_) {
      iter->DebugDump(4);
    }
    std::cout << "  indexes: " << std::endl;
    for (const auto& iter : indexes_) {
      iter->DebugDump(4);
    }
    std::cout << "--------INTERNAL TABLE--------" << std::endl;
    std::cout << "------TABLE_SHARE------" << std::endl;
    std::cout << "fields: " << s_fields_ << std::endl
              << "null_fields: " << s_null_fields_ << std::endl;
    std::cout << "------SE------" << std::endl;
    std::cout << "id: "
              << ib_id_ << std::endl
              << "n_cols: "
              << ib_n_cols_ << std::endl
              << "n_v_cols: "
              << ib_n_v_cols_ << std::endl
              << "n_m_v_cols: "
              << ib_n_m_v_cols_ << std::endl
              << "n_t_cols: "
              << ib_n_t_cols_ << std::endl
              << "n_instant_cols: "
              << ib_n_instant_cols_ << std::endl
              << "m_upgraded_instant: "
              << ib_m_upgraded_instant_ << std::endl
              << "initial_col_count: "
              << ib_initial_col_count_ << std::endl
              << "current_col_count: "
              << ib_current_col_count_ << std::endl
              << "total_col_count: "
              << ib_total_col_count_ << std::endl
              << "current_row_version: "
              << ib_current_row_version_ << std::endl
              << "n_def: "
              << ib_n_def_ << std::endl
              << "n_v_def: "
              << ib_n_v_def_ << std::endl
              << "cols:" << std::endl;
    for (auto* iter : ib_cols_) {
      std::cout << "  " << "------" << std::endl
                << "  " << "name: "
                << iter->name() << std::endl;
      std::cout << "  " << "------SHARE_TABLE------" << std::endl
                << "  " << "Field::type(): "
                << iter->FieldType() << std::endl
                << "  " << "Field::binary(): "
                << iter->IsBinary() << std::endl;
      std::cout << "  " << "------SE------" << std::endl
                << "  " << "ind: "
                << iter->ib_ind() << std::endl
                << "  " << "mtype: "
                << iter->ib_mtype() << std::endl
                << "  " << "is_visible: "
                << iter->ib_is_visible() << std::endl
                << "  " << "version_added: "
                << iter->ib_version_added() << std::endl
                << "  " << "version_dropped: "
                << iter->ib_version_dropped() << std::endl
                << "  " << "phy_pos: "
                << iter->ib_phy_pos() << std::endl
                << "  " << "col_len: "
                << iter->ib_col_len() << std::endl
                << "  " << "instant_default: "
                << iter->ib_instant_default() << std::endl;
    }
    std::cout << "indexes:" << std::endl;
    for (auto* iter : indexes_) {
      std::cout << "  " << "------" << std::endl;
      std::cout << "  " << "name: "
                << iter->name() << std::endl
                << "  " << "------TABLE SHARE------" << std::endl
                << "  " << "user_defined_key_parts: "
                << iter->s_user_defined_key_parts() << std::endl
                << "  " << "key_length: "
                << iter->s_key_length() << std::endl
                << "  " << "flags: "
                << iter->s_flags() << std::endl;
      std::cout << "  " << "------SE------" << std::endl
                << "  " << "id: "
                << iter->ib_id() << std::endl
                << "  " << "page: "
                << iter->ib_page() << std::endl
                << "  " << "n_fields: "
                << iter->ib_n_fields() << std::endl
                << "  " << "n_uniq: "
                << iter->ib_n_uniq() << std::endl
                << "  " << "type: "
                << iter->ib_type() << std::endl
                << "  " << "n_def: "
                << iter->ib_n_def() << std::endl
                << "  " << "n_nullable: "
                << iter->ib_n_nullable() << std::endl
                << "  " << "row_versions: "
                << iter->ib_row_versions() << std::endl
                << "  " << "instant_cols: "
                << iter->ib_instant_cols() << std::endl
                << "  " << "n_instant_nullable: "
                << iter->ib_n_instant_nullable() << std::endl
                << "  " << "n_total_fields: "
                << iter->ib_n_total_fields() << std::endl
                << "  " << "ib_fields_array: " << std::endl
                << "    ";
      if (HasRowVersions() && iter->IsClustered()) {
        for (uint32_t i = 0; i < iter->ib_n_def(); i++) {
          std::cout << (*(iter->ib_fields_array()))[i] << " ";
        }
        std::cout << std::endl;
      } else {
        std::cout << "NULL" << std::endl;
      }
      std::cout << "  " << "ib_nullables: " << std::endl
                << "    ";
      if (HasRowVersions() && iter->IsClustered()) {
        for (uint32_t i = 0; i < ib_current_row_version_; i++) {
          std::cout << (iter->ib_nullables())[i] << " ";
        }
        std::cout << std::endl;
      } else {
        std::cout << "NULL" << std::endl;
      }
      std::cout << "  " << "fields: " << std::endl;
      for (auto* field : *(iter->ib_fields())) {
        std::cout << "    " << "------" << std::endl
                  << "    " << "name: "
                  << field->column()->name() << std::endl
                  << "    " << "fixed_len: "
                  << field->ib_fixed_len() << std::endl
                  << "    " << "phy_pos: "
                  << field->column()->ib_phy_pos() << std::endl;
      }
    }
  }
  enum enum_hidden_type {
    HT_VISIBLE = 1,
    HT_HIDDEN_SYSTEM,
    HT_HIDDEN_SE,
    HT_HIDDEN_DDL
  };

  enum enum_row_format {
    RF_FIXED = 1,
    RF_DYNAMIC,
    RF_COMPRESSED,
    RF_REDUNDANT,
    RF_COMPACT,
    RF_PAGED
  };
  std::string RowFormatString() {
    switch (dd_row_format_) {
      case RF_FIXED:
        return "FIXED";
      case RF_DYNAMIC:
        return "DYNAMIC";
      case RF_COMPRESSED:
        return "COMPRESSED";
      case RF_REDUNDANT:
        return "REDUNDANT";
      case RF_COMPACT:
        return "COMPACT";
      case RF_PAGED:
        return "PAGED";
      default:
        return "UNKNOWN";
    }
  }

  enum enum_partition_type {
    PT_NONE = 0,
    PT_HASH,
    PT_KEY_51,
    PT_KEY_55,
    PT_LINEAR_HASH,
    PT_LINEAR_KEY_51,
    PT_LINEAR_KEY_55,
    PT_RANGE,
    PT_LIST,
    PT_RANGE_COLUMNS,
    PT_LIST_COLUMNS,
    PT_AUTO,
    PT_AUTO_LINEAR,
  };

  enum enum_subpartition_type {
    ST_NONE = 0,
    ST_HASH,
    ST_KEY_51,
    ST_KEY_55,
    ST_LINEAR_HASH,
    ST_LINEAR_KEY_51,
    ST_LINEAR_KEY_55
  };

  enum enum_default_partitioning {
    DP_NONE = 0,
    DP_NO,
    DP_YES,
    DP_NUMBER
  };

  static const std::set<std::string> default_valid_option_keys;

  const std::string& name() const {
    return dd_name_;
  }
  enum_hidden_type hidden() {
    return dd_hidden_;
  }
  std::string& schema_ref() {
    return dd_schema_ref_;
  }
  uint64_t se_private_id() {
    return dd_se_private_id_;
  }
  enum_row_format row_format() {
    return dd_row_format_;
  }
  enum_partition_type partition_type() {
    return dd_partition_type_;
  }
  uint32_t ib_id() {
    return ib_id_;
  }
  uint32_t ib_n_cols() {
    return ib_n_cols_;
  }
  std::vector<Column*>* ib_cols() {
    return &ib_cols_;
  }
  bool ib_is_system_table() {
    return ib_is_system_table_;
  }
  uint32_t ib_current_row_version() {
    return ib_current_row_version_;
  }
  bool ib_m_upgraded_instant() {
    return ib_m_upgraded_instant_;
  }
  std::vector<Index*>& indexes() {
    return indexes_;
  }
  Index* clust_index() {
    return clust_index_;
  }
  void set_clust_index(Index* index) {
    clust_index_ = index;
  }
  bool HasRowVersions() {
    return (ib_current_row_version_ > 0);
  }
  uint32_t GetTotalCols() {
    if (!HasRowVersions()) {
      return ib_n_cols_;
    }
    assert(ib_total_col_count_ + DATA_N_SYS_COLS ==
           ib_n_cols_ + GetNInstantDropCols());
    return (ib_n_cols_ + GetNInstantDropCols());
  }
  uint32_t GetNInstantAddCols() {
    return ib_total_col_count_ - ib_initial_col_count_;
  }
  bool HasInstantAddCols() {
    return GetNInstantAddCols() > 0;
  }
  uint32_t GetNInstantDropCols() {
    return ib_total_col_count_ - ib_current_col_count_;
  }
  bool HasInstantDropCols() {
    return GetNInstantDropCols() > 0;
  }
  uint32_t GetNInstantAddedColV1() {
    uint32_t n_cols_dropped = GetNInstantDropCols();
    uint32_t n_cols_added = GetNInstantAddCols();
    uint32_t n_instant_added_cols =
      ib_n_cols_ + n_cols_dropped - n_cols_added - ib_n_instant_cols_;
    return n_instant_added_cols;
  }
  bool IsCompact() {
    return (dd_row_format_ != enum_row_format::RF_REDUNDANT);
  }
  bool HasInstantCols() {
    if (ib_m_upgraded_instant_ || ib_n_instant_cols_ < ib_n_cols_) {
      return true;
    } else {
      return false;
    }
  }

  bool IsTableSupported();
  std::string UnsupportedReason();
  bool IsTableParsingRecSupported();

 private:
  explicit Table(unsigned char* sdi_data) : sdi_data_(sdi_data),
            dd_options_(default_valid_option_keys),
            dd_se_private_data_(),
            s_fields_(0), s_null_fields_(0),
            unsupported_reason_(0),
            clust_index_(nullptr) {
    columns_.clear();
    indexes_.clear();
  }
  bool Init(const rapidjson::Value& dd_obj);
  unsigned char* sdi_data_;
  /* DD */
  std::string dd_name_;
  uint32_t dd_mysql_version_id_;
  uint64_t dd_created_;
  uint64_t dd_last_altered_;
  enum_hidden_type dd_hidden_;
  Properties dd_options_;
  std::vector<Column*> columns_;
  std::string dd_schema_ref_;
  uint64_t dd_se_private_id_;
  std::string dd_engine_;
  std::string dd_comment_;
  uint32_t dd_last_checked_for_upgrade_version_id_;
  Properties dd_se_private_data_;
  std::string dd_engine_attribute_;
  std::string dd_secondary_engine_attribute_;
  enum_row_format dd_row_format_;
  enum_partition_type dd_partition_type_;
  std::string dd_partition_expression_;
  std::string dd_partition_expression_utf8_;
  enum_default_partitioning dd_default_partitioning_;
  enum_subpartition_type dd_subpartition_type_;
  std::string dd_subpartition_expression_;
  std::string dd_subpartition_expression_utf8_;
  enum_default_partitioning dd_default_subpartitioning_;
  std::vector<Index*> indexes_;
  // dd_foreign_keys
  // dd_check_constraints
  // dd_partitions
  uint64_t dd_collation_id_;

  /* TABLE_SHARE */
  uint32_t s_fields_;
  uint32_t s_null_fields_;
  std::vector<Column*> s_field_;

  /* SE */
  Column* FindColumn(const std::string& col_name);
  void PreCheck();
  bool InitSeTable();
  bool ContainFulltext();
  uint32_t unsupported_reason_;
  uint32_t ib_id_;
  uint32_t ib_n_cols_;
  uint32_t ib_n_v_cols_;
  uint32_t ib_n_m_v_cols_;
  uint32_t ib_n_t_cols_;
  uint32_t ib_n_instant_cols_;
  bool ib_m_upgraded_instant_;
  uint32_t ib_initial_col_count_;
  uint32_t ib_current_col_count_;
  uint32_t ib_total_col_count_;
  uint32_t ib_current_row_version_;

  uint32_t ib_n_def_;
  uint32_t ib_n_v_def_;
  uint32_t ib_n_t_def_;
  std::vector<Column*> ib_cols_;
  Column* row_id_col_;

  bool ib_is_system_table_;
  Index* clust_index_;
};

struct PageAnalysisResult;
class Record {
 public:
  Record(const unsigned char* rec, Index* index) :
    rec_(rec), index_(index), offsets_(nullptr) {
  }
  ~Record() {
    if (offsets_ != nullptr) {
      delete [] offsets_;
    }
  }
  uint32_t GetStatus();
  uint32_t* GetColumnOffsets();
  uint32_t GetChildPageNo();
  void ParseRecord(bool leaf, uint32_t row_no,
                   PageAnalysisResult* result,
                   bool print);

 private:
  uint32_t GetBitsFrom1B(uint32_t offs, uint32_t mask, uint32_t shift);
  uint32_t GetBitsFrom2B(uint32_t offs, uint32_t mask, uint32_t shift);
  void SetNAlloc(uint32_t n_alloc) {
    assert(offsets_);
    offsets_[0] = n_alloc;
  }
  void SetNFields(uint32_t n_fields) {
    assert(offsets_);
    offsets_[1] = n_fields;
  }
  uint32_t GetNAlloc() {
    assert(offsets_);
    return offsets_[0];
  }
  uint32_t GetNFields() {
    assert(offsets_);
    return offsets_[1];
  }
  enum REC_INSERT_STATE {
    INSERTED_BEFORE_INSTANT_ADD_OLD_IMPLEMENTATION,
    INSERTED_AFTER_INSTANT_ADD_OLD_IMPLEMENTATION,
    INSERTED_AFTER_UPGRADE_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION,
    INSERTED_BEFORE_INSTANT_ADD_NEW_IMPLEMENTATION,
    INSERTED_AFTER_INSTANT_ADD_NEW_IMPLEMENTATION,
    INSERTED_INTO_TABLE_WITH_NO_INSTANT_NO_VERSION,
    NONE
  };

  void InitColumnOffsets();
  void InitColumnOffsetsCompact();
  void InitColumnOffsetsCompactLeaf();
  bool IsVersionedCompact();
  bool GetInstantFlagCompact();
  REC_INSERT_STATE InitNullAndLengthCompact(const unsigned char** nulls,
                                            const unsigned char** lens,
                                            uint16_t* n_null,
                                            uint16_t* non_default_fields,
                                            uint8_t* row_version);

  REC_INSERT_STATE GetInsertState();
  uint32_t GetInfoBits(bool comp);
  uint32_t GetNFieldsInstant(const uint32_t extra_bytes,
                             uint16_t* length);
  uint64_t GetInstantOffset(uint32_t n, uint64_t offs);
  const unsigned char* rec_;
  Index* index_;
  uint32_t* offsets_;
};

class ibdNinja {
 public:
  static ibdNinja* CreateNinja(const char* idb_filename);
  ~ibdNinja() {
    for (auto iter : all_tables_) {
      delete iter;
    }
  }

  const std::map<uint64_t, Table*>* tables() const {
    return &tables_;
  }
  const std::map<uint64_t, Index*>* indexes() const {
    return &indexes_;
  }

  void AddTable(Table* table);
  Table* GetTable(const std::string& db_name, const std::string& table_name) {
    Table* tab = nullptr;
    for (auto iter : tables_) {
      if (iter.second->schema_ref() == db_name &&
          iter.second->name() == table_name) {
        tab = iter.second;
        break;
      }
    }
    return tab;
  }
  Table* GetTable(uint64_t table_id) {
    Table* tab = nullptr;
    for (auto iter : tables_) {
      if (iter.first == table_id) {
        tab = iter.second;
        break;
      }
    }
    return tab;
  }
  Index* GetIndex(uint64_t index_id) {
    Index* idx = nullptr;
    for (auto iter : indexes_) {
      if (iter.first == index_id) {
        idx = iter.second;
        break;
      }
    }
    return idx;
  }

  static ssize_t ReadPage(uint32_t page_no, unsigned char* buf);
  bool ParsePage(uint32_t page_no,
                 PageAnalysisResult* result_aggr,
                 bool print,
                 bool print_record);
  bool ParseIndex(uint32_t index_id);

  bool ParseTable(uint32_t table_id);

  void ShowTables(bool only_supported);
  void ShowLeftmostPages(uint32_t index_id);
  static const char* g_version_;
  static void PrintName();

 private:
  explicit ibdNinja(uint32_t n_pages) : n_pages_(n_pages) {
    all_tables_.clear();
    tables_.clear();
    indexes_.clear();
  }
  static bool SDIToLeftmostLeaf(unsigned char* buf, uint32_t sdi_root,
                                uint32_t* leaf_page_no);
  static uint64_t SDIFetchUncompBlob(uint32_t first_blob_page_no,
                                     uint64_t total_off_page_length,
                                     unsigned char* dest_buf,
                                     uint32_t* n_ext_pages,
                                     bool* error);
  static unsigned char* SDIGetFirstUserRec(unsigned char* buf,
                                           uint32_t buf_len);
  static unsigned char* SDIGetNextRec(unsigned char* current_rec,
                                      unsigned char* buf,
                                      uint32_t buf_len,
                                      bool* corrupt);
  static bool SDIParseRec(unsigned char* rec,
                          uint64_t* sdi_type, uint64_t* sdi_id,
                          unsigned char** sdi_data, uint64_t* sdi_data_len);

  static unsigned char* GetFirstUserRec(unsigned char* buf);
  static unsigned char* GetNextRecInPage(unsigned char* current_rec,
                                         unsigned char* buf,
                                         bool* corrupt);
  static bool ToLeftmostLeaf(Index* index,
                             unsigned char* buf, uint32_t root,
                             std::vector<uint32_t>* leaf_pages_no);
  bool ParseIndex(Index* index);

  uint32_t n_pages_;
  std::vector<Table*> all_tables_;
  std::map<uint64_t, Table*> tables_;
  std::map<uint64_t, Index*> indexes_;
};

}  // namespace ibd_ninja

#endif  // IBDNINJA_H_
