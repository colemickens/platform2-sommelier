// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipp/ipp_collections.h"

namespace ipp {
std::vector<Attribute*> C_cover_back::GetKnownAttributes() {
  return {&cover_type, &media, &media_col};
}
std::vector<const Attribute*> C_cover_back::GetKnownAttributes() const {
  return {&cover_type, &media, &media_col};
}
const std::map<AttrName, AttrDef> C_cover_back::defs_{
    {AttrName::cover_type, {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}}};
std::vector<Attribute*>
C_document_format_details_default::GetKnownAttributes() {
  return {&document_format,
          &document_format_device_id,
          &document_format_version,
          &document_natural_language,
          &document_source_application_name,
          &document_source_application_version,
          &document_source_os_name,
          &document_source_os_version};
}
std::vector<const Attribute*>
C_document_format_details_default::GetKnownAttributes() const {
  return {&document_format,
          &document_format_device_id,
          &document_format_version,
          &document_natural_language,
          &document_source_application_name,
          &document_source_application_version,
          &document_source_os_name,
          &document_source_os_version};
}
const std::map<AttrName, AttrDef> C_document_format_details_default::defs_{
    {AttrName::document_format,
     {AttrType::mimeMediaType, InternalType::kString, false}},
    {AttrName::document_format_device_id,
     {AttrType::text, InternalType::kStringWithLanguage, false}},
    {AttrName::document_format_version,
     {AttrType::text, InternalType::kStringWithLanguage, false}},
    {AttrName::document_natural_language,
     {AttrType::naturalLanguage, InternalType::kString, true}},
    {AttrName::document_source_application_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::document_source_application_version,
     {AttrType::text, InternalType::kStringWithLanguage, false}},
    {AttrName::document_source_os_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::document_source_os_version,
     {AttrType::text, InternalType::kStringWithLanguage, false}}};
std::vector<Attribute*> C_finishings_col::GetKnownAttributes() {
  return {&baling,
          &binding,
          &coating,
          &covering,
          &finishing_template,
          &folding,
          &imposition_template,
          &laminating,
          &media_sheets_supported,
          &media_size,
          &media_size_name,
          &punching,
          &stitching,
          &trimming};
}
std::vector<const Attribute*> C_finishings_col::GetKnownAttributes() const {
  return {&baling,
          &binding,
          &coating,
          &covering,
          &finishing_template,
          &folding,
          &imposition_template,
          &laminating,
          &media_sheets_supported,
          &media_size,
          &media_size_name,
          &punching,
          &stitching,
          &trimming};
}
const std::map<AttrName, AttrDef> C_finishings_col::defs_{
    {AttrName::baling,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_baling(); }}},
    {AttrName::binding,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_binding(); }}},
    {AttrName::coating,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_coating(); }}},
    {AttrName::covering,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_covering(); }}},
    {AttrName::finishing_template,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::folding,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new C_folding(); }}},
    {AttrName::imposition_template,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::laminating,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_laminating(); }}},
    {AttrName::media_sheets_supported,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, false}},
    {AttrName::media_size,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_size(); }}},
    {AttrName::media_size_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::punching,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_punching(); }}},
    {AttrName::stitching,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_stitching(); }}},
    {AttrName::trimming,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new C_trimming(); }}}};
std::vector<Attribute*> C_finishings_col::C_baling::GetKnownAttributes() {
  return {&baling_type, &baling_when};
}
std::vector<const Attribute*> C_finishings_col::C_baling::GetKnownAttributes()
    const {
  return {&baling_type, &baling_when};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_baling::defs_{
    {AttrName::baling_type, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::baling_when,
     {AttrType::keyword, InternalType::kInteger, false}}};
std::vector<Attribute*> C_finishings_col::C_binding::GetKnownAttributes() {
  return {&binding_reference_edge, &binding_type};
}
std::vector<const Attribute*> C_finishings_col::C_binding::GetKnownAttributes()
    const {
  return {&binding_reference_edge, &binding_type};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_binding::defs_{
    {AttrName::binding_reference_edge,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::binding_type,
     {AttrType::keyword, InternalType::kString, false}}};
std::vector<Attribute*> C_finishings_col::C_coating::GetKnownAttributes() {
  return {&coating_sides, &coating_type};
}
std::vector<const Attribute*> C_finishings_col::C_coating::GetKnownAttributes()
    const {
  return {&coating_sides, &coating_type};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_coating::defs_{
    {AttrName::coating_sides,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::coating_type,
     {AttrType::keyword, InternalType::kString, false}}};
std::vector<Attribute*> C_finishings_col::C_covering::GetKnownAttributes() {
  return {&covering_name};
}
std::vector<const Attribute*> C_finishings_col::C_covering::GetKnownAttributes()
    const {
  return {&covering_name};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_covering::defs_{
    {AttrName::covering_name,
     {AttrType::keyword, InternalType::kString, false}}};
std::vector<Attribute*> C_finishings_col::C_folding::GetKnownAttributes() {
  return {&folding_direction, &folding_offset, &folding_reference_edge};
}
std::vector<const Attribute*> C_finishings_col::C_folding::GetKnownAttributes()
    const {
  return {&folding_direction, &folding_offset, &folding_reference_edge};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_folding::defs_{
    {AttrName::folding_direction,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::folding_offset,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::folding_reference_edge,
     {AttrType::keyword, InternalType::kInteger, false}}};
std::vector<Attribute*> C_finishings_col::C_laminating::GetKnownAttributes() {
  return {&laminating_sides, &laminating_type};
}
std::vector<const Attribute*>
C_finishings_col::C_laminating::GetKnownAttributes() const {
  return {&laminating_sides, &laminating_type};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_laminating::defs_{
    {AttrName::laminating_sides,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::laminating_type,
     {AttrType::keyword, InternalType::kString, false}}};
std::vector<Attribute*> C_finishings_col::C_media_size::GetKnownAttributes() {
  return {};
}
std::vector<const Attribute*>
C_finishings_col::C_media_size::GetKnownAttributes() const {
  return {};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_media_size::defs_{};
std::vector<Attribute*> C_finishings_col::C_punching::GetKnownAttributes() {
  return {&punching_locations, &punching_offset, &punching_reference_edge};
}
std::vector<const Attribute*> C_finishings_col::C_punching::GetKnownAttributes()
    const {
  return {&punching_locations, &punching_offset, &punching_reference_edge};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_punching::defs_{
    {AttrName::punching_locations,
     {AttrType::integer, InternalType::kInteger, true}},
    {AttrName::punching_offset,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::punching_reference_edge,
     {AttrType::keyword, InternalType::kInteger, false}}};
std::vector<Attribute*> C_finishings_col::C_stitching::GetKnownAttributes() {
  return {&stitching_angle, &stitching_locations, &stitching_method,
          &stitching_offset, &stitching_reference_edge};
}
std::vector<const Attribute*>
C_finishings_col::C_stitching::GetKnownAttributes() const {
  return {&stitching_angle, &stitching_locations, &stitching_method,
          &stitching_offset, &stitching_reference_edge};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_stitching::defs_{
    {AttrName::stitching_angle,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::stitching_locations,
     {AttrType::integer, InternalType::kInteger, true}},
    {AttrName::stitching_method,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::stitching_offset,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::stitching_reference_edge,
     {AttrType::keyword, InternalType::kInteger, false}}};
std::vector<Attribute*> C_finishings_col::C_trimming::GetKnownAttributes() {
  return {&trimming_offset, &trimming_reference_edge, &trimming_type,
          &trimming_when};
}
std::vector<const Attribute*> C_finishings_col::C_trimming::GetKnownAttributes()
    const {
  return {&trimming_offset, &trimming_reference_edge, &trimming_type,
          &trimming_when};
}
const std::map<AttrName, AttrDef> C_finishings_col::C_trimming::defs_{
    {AttrName::trimming_offset,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::trimming_reference_edge,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::trimming_type,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::trimming_when,
     {AttrType::keyword, InternalType::kInteger, false}}};
std::vector<Attribute*> C_insert_sheet::GetKnownAttributes() {
  return {&insert_after_page_number, &insert_count, &media, &media_col};
}
std::vector<const Attribute*> C_insert_sheet::GetKnownAttributes() const {
  return {&insert_after_page_number, &insert_count, &media, &media_col};
}
const std::map<AttrName, AttrDef> C_insert_sheet::defs_{
    {AttrName::insert_after_page_number,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::insert_count,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}}};
std::vector<Attribute*> C_job_accounting_sheets::GetKnownAttributes() {
  return {&job_accounting_output_bin, &job_accounting_sheets_type, &media,
          &media_col};
}
std::vector<const Attribute*> C_job_accounting_sheets::GetKnownAttributes()
    const {
  return {&job_accounting_output_bin, &job_accounting_sheets_type, &media,
          &media_col};
}
const std::map<AttrName, AttrDef> C_job_accounting_sheets::defs_{
    {AttrName::job_accounting_output_bin,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::job_accounting_sheets_type,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}}};
std::vector<Attribute*> C_job_constraints_supported::GetKnownAttributes() {
  return {&resolver_name};
}
std::vector<const Attribute*> C_job_constraints_supported::GetKnownAttributes()
    const {
  return {&resolver_name};
}
const std::map<AttrName, AttrDef> C_job_constraints_supported::defs_{
    {AttrName::resolver_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}}};
std::vector<Attribute*> C_job_error_sheet::GetKnownAttributes() {
  return {&job_error_sheet_type, &job_error_sheet_when, &media, &media_col};
}
std::vector<const Attribute*> C_job_error_sheet::GetKnownAttributes() const {
  return {&job_error_sheet_type, &job_error_sheet_when, &media, &media_col};
}
const std::map<AttrName, AttrDef> C_job_error_sheet::defs_{
    {AttrName::job_error_sheet_type,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::job_error_sheet_when,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}}};
std::vector<Attribute*> C_job_finishings_col_actual::GetKnownAttributes() {
  return {&media_back_coating,  &media_bottom_margin, &media_color,
          &media_front_coating, &media_grain,         &media_hole_count,
          &media_info,          &media_key,           &media_left_margin,
          &media_order_count,   &media_pre_printed,   &media_recycled,
          &media_right_margin,  &media_size,          &media_size_name,
          &media_source,        &media_thickness,     &media_tooth,
          &media_top_margin,    &media_type,          &media_weight_metric};
}
std::vector<const Attribute*> C_job_finishings_col_actual::GetKnownAttributes()
    const {
  return {&media_back_coating,  &media_bottom_margin, &media_color,
          &media_front_coating, &media_grain,         &media_hole_count,
          &media_info,          &media_key,           &media_left_margin,
          &media_order_count,   &media_pre_printed,   &media_recycled,
          &media_right_margin,  &media_size,          &media_size_name,
          &media_source,        &media_thickness,     &media_tooth,
          &media_top_margin,    &media_type,          &media_weight_metric};
}
const std::map<AttrName, AttrDef> C_job_finishings_col_actual::defs_{
    {AttrName::media_back_coating,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_bottom_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_color, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_front_coating,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_grain, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_hole_count,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_info,
     {AttrType::text, InternalType::kStringWithLanguage, false}},
    {AttrName::media_key, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_left_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_order_count,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_pre_printed,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_recycled,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_right_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_size,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_size(); }}},
    {AttrName::media_size_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::media_source, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_thickness,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_tooth, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_top_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_type, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_weight_metric,
     {AttrType::integer, InternalType::kInteger, false}}};
std::vector<Attribute*>
C_job_finishings_col_actual::C_media_size::GetKnownAttributes() {
  return {&x_dimension, &y_dimension};
}
std::vector<const Attribute*>
C_job_finishings_col_actual::C_media_size::GetKnownAttributes() const {
  return {&x_dimension, &y_dimension};
}
const std::map<AttrName, AttrDef>
    C_job_finishings_col_actual::C_media_size::defs_{
        {AttrName::x_dimension,
         {AttrType::integer, InternalType::kInteger, false}},
        {AttrName::y_dimension,
         {AttrType::integer, InternalType::kInteger, false}}};
std::vector<Attribute*> C_job_save_disposition::GetKnownAttributes() {
  return {&save_disposition, &save_info};
}
std::vector<const Attribute*> C_job_save_disposition::GetKnownAttributes()
    const {
  return {&save_disposition, &save_info};
}
const std::map<AttrName, AttrDef> C_job_save_disposition::defs_{
    {AttrName::save_disposition,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::save_info,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new C_save_info(); }}}};
std::vector<Attribute*>
C_job_save_disposition::C_save_info::GetKnownAttributes() {
  return {&save_document_format, &save_location, &save_name};
}
std::vector<const Attribute*>
C_job_save_disposition::C_save_info::GetKnownAttributes() const {
  return {&save_document_format, &save_location, &save_name};
}
const std::map<AttrName, AttrDef> C_job_save_disposition::C_save_info::defs_{
    {AttrName::save_document_format,
     {AttrType::mimeMediaType, InternalType::kString, false}},
    {AttrName::save_location, {AttrType::uri, InternalType::kString, false}},
    {AttrName::save_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}}};
std::vector<Attribute*> C_job_sheets_col::GetKnownAttributes() {
  return {&job_sheets, &media, &media_col};
}
std::vector<const Attribute*> C_job_sheets_col::GetKnownAttributes() const {
  return {&job_sheets, &media, &media_col};
}
const std::map<AttrName, AttrDef> C_job_sheets_col::defs_{
    {AttrName::job_sheets, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}}};
std::vector<Attribute*> C_media_col_database::GetKnownAttributes() {
  return {&media_back_coating,  &media_bottom_margin,
          &media_color,         &media_front_coating,
          &media_grain,         &media_hole_count,
          &media_info,          &media_key,
          &media_left_margin,   &media_order_count,
          &media_pre_printed,   &media_recycled,
          &media_right_margin,  &media_size,
          &media_size_name,     &media_source,
          &media_thickness,     &media_tooth,
          &media_top_margin,    &media_type,
          &media_weight_metric, &media_source_properties};
}
std::vector<const Attribute*> C_media_col_database::GetKnownAttributes() const {
  return {&media_back_coating,  &media_bottom_margin,
          &media_color,         &media_front_coating,
          &media_grain,         &media_hole_count,
          &media_info,          &media_key,
          &media_left_margin,   &media_order_count,
          &media_pre_printed,   &media_recycled,
          &media_right_margin,  &media_size,
          &media_size_name,     &media_source,
          &media_thickness,     &media_tooth,
          &media_top_margin,    &media_type,
          &media_weight_metric, &media_source_properties};
}
const std::map<AttrName, AttrDef> C_media_col_database::defs_{
    {AttrName::media_back_coating,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_bottom_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_color, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_front_coating,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_grain, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_hole_count,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_info,
     {AttrType::text, InternalType::kStringWithLanguage, false}},
    {AttrName::media_key, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_left_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_order_count,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_pre_printed,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_recycled,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_right_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_size,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_size(); }}},
    {AttrName::media_size_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::media_source, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_thickness,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_tooth, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_top_margin,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_type, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_weight_metric,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::media_source_properties,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_source_properties(); }}}};
std::vector<Attribute*>
C_media_col_database::C_media_size::GetKnownAttributes() {
  return {&x_dimension, &y_dimension};
}
std::vector<const Attribute*>
C_media_col_database::C_media_size::GetKnownAttributes() const {
  return {&x_dimension, &y_dimension};
}
const std::map<AttrName, AttrDef> C_media_col_database::C_media_size::defs_{
    {AttrName::x_dimension, {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::y_dimension,
     {AttrType::integer, InternalType::kInteger, false}}};
std::vector<Attribute*>
C_media_col_database::C_media_source_properties::GetKnownAttributes() {
  return {&media_source_feed_direction, &media_source_feed_orientation};
}
std::vector<const Attribute*>
C_media_col_database::C_media_source_properties::GetKnownAttributes() const {
  return {&media_source_feed_direction, &media_source_feed_orientation};
}
const std::map<AttrName, AttrDef>
    C_media_col_database::C_media_source_properties::defs_{
        {AttrName::media_source_feed_direction,
         {AttrType::keyword, InternalType::kInteger, false}},
        {AttrName::media_source_feed_orientation,
         {AttrType::enum_, InternalType::kInteger, false}}};
std::vector<Attribute*> C_media_size_supported::GetKnownAttributes() {
  return {&x_dimension, &y_dimension};
}
std::vector<const Attribute*> C_media_size_supported::GetKnownAttributes()
    const {
  return {&x_dimension, &y_dimension};
}
const std::map<AttrName, AttrDef> C_media_size_supported::defs_{
    {AttrName::x_dimension,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, false}},
    {AttrName::y_dimension,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, false}}};
std::vector<Attribute*> C_overrides::GetKnownAttributes() {
  return {&job_account_id,
          &job_account_type,
          &job_accounting_sheets,
          &job_accounting_user_id,
          &job_copies,
          &job_cover_back,
          &job_cover_front,
          &job_delay_output_until,
          &job_delay_output_until_time,
          &job_error_action,
          &job_error_sheet,
          &job_finishings,
          &job_finishings_col,
          &job_hold_until,
          &job_hold_until_time,
          &job_message_to_operator,
          &job_pages_per_set,
          &job_phone_number,
          &job_priority,
          &job_recipient_name,
          &job_save_disposition,
          &job_sheet_message,
          &job_sheets,
          &job_sheets_col,
          &pages_per_subset,
          &output_bin,
          &output_device,
          &multiple_document_handling,
          &y_side1_image_shift,
          &y_side2_image_shift,
          &number_up,
          &orientation_requested,
          &page_delivery,
          &page_order_received,
          &page_ranges,
          &pdl_init_file,
          &print_color_mode,
          &print_content_optimize,
          &print_quality,
          &print_rendering_intent,
          &printer_resolution,
          &presentation_direction_number_up,
          &media,
          &sides,
          &x_image_position,
          &x_image_shift,
          &x_side1_image_shift,
          &x_side2_image_shift,
          &y_image_position,
          &y_image_shift,
          &copies,
          &cover_back,
          &cover_front,
          &imposition_template,
          &insert_sheet,
          &media_col,
          &media_input_tray_check,
          &print_scaling,
          &proof_print,
          &separator_sheets,
          &sheet_collate,
          &feed_orientation,
          &finishings,
          &finishings_col,
          &font_name_requested,
          &font_size_requested,
          &force_front_side,
          &document_copies,
          &document_numbers,
          &pages};
}
std::vector<const Attribute*> C_overrides::GetKnownAttributes() const {
  return {&job_account_id,
          &job_account_type,
          &job_accounting_sheets,
          &job_accounting_user_id,
          &job_copies,
          &job_cover_back,
          &job_cover_front,
          &job_delay_output_until,
          &job_delay_output_until_time,
          &job_error_action,
          &job_error_sheet,
          &job_finishings,
          &job_finishings_col,
          &job_hold_until,
          &job_hold_until_time,
          &job_message_to_operator,
          &job_pages_per_set,
          &job_phone_number,
          &job_priority,
          &job_recipient_name,
          &job_save_disposition,
          &job_sheet_message,
          &job_sheets,
          &job_sheets_col,
          &pages_per_subset,
          &output_bin,
          &output_device,
          &multiple_document_handling,
          &y_side1_image_shift,
          &y_side2_image_shift,
          &number_up,
          &orientation_requested,
          &page_delivery,
          &page_order_received,
          &page_ranges,
          &pdl_init_file,
          &print_color_mode,
          &print_content_optimize,
          &print_quality,
          &print_rendering_intent,
          &printer_resolution,
          &presentation_direction_number_up,
          &media,
          &sides,
          &x_image_position,
          &x_image_shift,
          &x_side1_image_shift,
          &x_side2_image_shift,
          &y_image_position,
          &y_image_shift,
          &copies,
          &cover_back,
          &cover_front,
          &imposition_template,
          &insert_sheet,
          &media_col,
          &media_input_tray_check,
          &print_scaling,
          &proof_print,
          &separator_sheets,
          &sheet_collate,
          &feed_orientation,
          &finishings,
          &finishings_col,
          &font_name_requested,
          &font_size_requested,
          &force_front_side,
          &document_copies,
          &document_numbers,
          &pages};
}
const std::map<AttrName, AttrDef> C_overrides::defs_{
    {AttrName::job_account_id,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::job_account_type,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::job_accounting_sheets,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_job_accounting_sheets(); }}},
    {AttrName::job_accounting_user_id,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::job_copies, {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::job_cover_back,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_job_cover_back(); }}},
    {AttrName::job_cover_front,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_job_cover_front(); }}},
    {AttrName::job_delay_output_until,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::job_delay_output_until_time,
     {AttrType::dateTime, InternalType::kDateTime, false}},
    {AttrName::job_error_action,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::job_error_sheet,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_job_error_sheet(); }}},
    {AttrName::job_finishings, {AttrType::enum_, InternalType::kInteger, true}},
    {AttrName::job_finishings_col,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new C_job_finishings_col(); }}},
    {AttrName::job_hold_until,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::job_hold_until_time,
     {AttrType::dateTime, InternalType::kDateTime, false}},
    {AttrName::job_message_to_operator,
     {AttrType::text, InternalType::kStringWithLanguage, false}},
    {AttrName::job_pages_per_set,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::job_phone_number, {AttrType::uri, InternalType::kString, false}},
    {AttrName::job_priority,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::job_recipient_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::job_save_disposition,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_job_save_disposition(); }}},
    {AttrName::job_sheet_message,
     {AttrType::text, InternalType::kStringWithLanguage, false}},
    {AttrName::job_sheets, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::job_sheets_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_job_sheets_col(); }}},
    {AttrName::pages_per_subset,
     {AttrType::integer, InternalType::kInteger, true}},
    {AttrName::output_bin, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::output_device,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::multiple_document_handling,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::y_side1_image_shift,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::y_side2_image_shift,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::number_up, {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::orientation_requested,
     {AttrType::enum_, InternalType::kInteger, false}},
    {AttrName::page_delivery,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::page_order_received,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::page_ranges,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, true}},
    {AttrName::pdl_init_file,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new C_pdl_init_file(); }}},
    {AttrName::print_color_mode,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::print_content_optimize,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::print_quality, {AttrType::enum_, InternalType::kInteger, false}},
    {AttrName::print_rendering_intent,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::printer_resolution,
     {AttrType::resolution, InternalType::kResolution, false}},
    {AttrName::presentation_direction_number_up,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::sides, {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::x_image_position,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::x_image_shift,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::x_side1_image_shift,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::x_side2_image_shift,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::y_image_position,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::y_image_shift,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::copies, {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::cover_back,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_cover_back(); }}},
    {AttrName::cover_front,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_cover_front(); }}},
    {AttrName::imposition_template,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::insert_sheet,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new C_insert_sheet(); }}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}},
    {AttrName::media_input_tray_check,
     {AttrType::keyword, InternalType::kString, false}},
    {AttrName::print_scaling,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::proof_print,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_proof_print(); }}},
    {AttrName::separator_sheets,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_separator_sheets(); }}},
    {AttrName::sheet_collate,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::feed_orientation,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::finishings, {AttrType::enum_, InternalType::kInteger, true}},
    {AttrName::finishings_col,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new C_finishings_col(); }}},
    {AttrName::font_name_requested,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::font_size_requested,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::force_front_side,
     {AttrType::integer, InternalType::kInteger, true}},
    {AttrName::document_copies,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, true}},
    {AttrName::document_numbers,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, true}},
    {AttrName::pages,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, true}}};
std::vector<Attribute*> C_pdl_init_file::GetKnownAttributes() {
  return {&pdl_init_file_entry, &pdl_init_file_location, &pdl_init_file_name};
}
std::vector<const Attribute*> C_pdl_init_file::GetKnownAttributes() const {
  return {&pdl_init_file_entry, &pdl_init_file_location, &pdl_init_file_name};
}
const std::map<AttrName, AttrDef> C_pdl_init_file::defs_{
    {AttrName::pdl_init_file_entry,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::pdl_init_file_location,
     {AttrType::uri, InternalType::kString, false}},
    {AttrName::pdl_init_file_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}}};
std::vector<Attribute*> C_printer_contact_col::GetKnownAttributes() {
  return {&contact_name, &contact_uri, &contact_vcard};
}
std::vector<const Attribute*> C_printer_contact_col::GetKnownAttributes()
    const {
  return {&contact_name, &contact_uri, &contact_vcard};
}
const std::map<AttrName, AttrDef> C_printer_contact_col::defs_{
    {AttrName::contact_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::contact_uri, {AttrType::uri, InternalType::kString, false}},
    {AttrName::contact_vcard,
     {AttrType::text, InternalType::kStringWithLanguage, true}}};
std::vector<Attribute*> C_printer_icc_profiles::GetKnownAttributes() {
  return {&profile_name, &profile_url};
}
std::vector<const Attribute*> C_printer_icc_profiles::GetKnownAttributes()
    const {
  return {&profile_name, &profile_url};
}
const std::map<AttrName, AttrDef> C_printer_icc_profiles::defs_{
    {AttrName::profile_name,
     {AttrType::name, InternalType::kStringWithLanguage, false}},
    {AttrName::profile_url, {AttrType::uri, InternalType::kString, false}}};
std::vector<Attribute*> C_printer_xri_supported::GetKnownAttributes() {
  return {&xri_authentication, &xri_security, &xri_uri};
}
std::vector<const Attribute*> C_printer_xri_supported::GetKnownAttributes()
    const {
  return {&xri_authentication, &xri_security, &xri_uri};
}
const std::map<AttrName, AttrDef> C_printer_xri_supported::defs_{
    {AttrName::xri_authentication,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::xri_security,
     {AttrType::keyword, InternalType::kInteger, false}},
    {AttrName::xri_uri, {AttrType::uri, InternalType::kString, false}}};
std::vector<Attribute*> C_proof_print::GetKnownAttributes() {
  return {&media, &media_col, &proof_print_copies};
}
std::vector<const Attribute*> C_proof_print::GetKnownAttributes() const {
  return {&media, &media_col, &proof_print_copies};
}
const std::map<AttrName, AttrDef> C_proof_print::defs_{
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}},
    {AttrName::proof_print_copies,
     {AttrType::integer, InternalType::kInteger, false}}};
std::vector<Attribute*> C_separator_sheets::GetKnownAttributes() {
  return {&media, &media_col, &separator_sheets_type};
}
std::vector<const Attribute*> C_separator_sheets::GetKnownAttributes() const {
  return {&media, &media_col, &separator_sheets_type};
}
const std::map<AttrName, AttrDef> C_separator_sheets::defs_{
    {AttrName::media, {AttrType::keyword, InternalType::kString, false}},
    {AttrName::media_col,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new C_media_col(); }}},
    {AttrName::separator_sheets_type,
     {AttrType::keyword, InternalType::kInteger, true}}};
}  // namespace ipp
