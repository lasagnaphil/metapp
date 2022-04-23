#ifndef LLVM_CLANG_TOOLS_EXTRA_METAPP_TOOLING_CLASSFINDER_HPP
#define LLVM_CLANG_TOOLS_EXTRA_METAPP_TOOLING_CLASSFINDER_HPP
#pragma once

#include "generator.hpp"
#include "generator_store.hpp"
#include "reflectedclass.hpp"
#include "utils.hpp"

#include <pugixml.hpp>

class ClassFinder : public MatchFinder::MatchCallback {
public:
  // ClassFinder() = default;
  ClassFinder(std::string fileName)
      : m_exportFileName{std::move(fileName)} {}

  virtual void run(MatchFinder::MatchResult const &result) override {
    m_context = result.Context;
    m_sourceman = result.SourceManager;

    clang::CXXRecordDecl const *record =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("id");
    if (record)
      return FoundRecord(record);

    clang::FieldDecl const *field =
        result.Nodes.getNodeAs<clang::FieldDecl>("id");
    if (field)
      return FoundField(field);

    clang::FunctionDecl const *function =
        result.Nodes.getNodeAs<clang::FunctionDecl>("id");
    if (function)
      return FoundFunction(function);
  }

  virtual void onStartOfTranslationUnit() override {}

  virtual void onEndOfTranslationUnit() override {
    // Export XML file
    pugi::xml_document doc;
    auto el_db = doc.append_child("ClassDB");
    for (auto &ref : m_classes) {
      auto el_class = el_db.append_child("Class");
      auto class_name = ref.m_record->getNameAsString();
      el_class.append_attribute("name") = class_name.c_str();
      if (ref.m_namespace) {
        auto namespace_name = ref.m_namespace->getNameAsString();
        el_class.append_attribute("namespace") = namespace_name.c_str();
      }
      for (auto& attr : ref.m_record->attrs()) {
        auto el_attr = el_class.append_child("Attribute");
        auto attr_name = attr->getAttrName()->getName().str();
        el_attr.append_attribute("name") = attr_name.c_str();
      }
      for (auto& base : ref.m_record->bases()) {
        auto base_class_name = base.getType().getAsString();
        el_class.append_attribute("base") = base_class_name.c_str();
        break; // Assume there is only one base class
      }
      for (auto& field : ref.m_fields) {
        auto el_field = el_class.append_child("Field");
        auto field_name = field->getNameAsString();
        el_field.append_attribute("name") = field_name.c_str();
        auto field_type = field->getType().getAsString();
        el_field.append_attribute("type") = field_type.c_str();
      }
      for (auto& fn : ref.m_functions) {
        auto el_fn = el_class.append_child("Function");
        auto fn_name = fn->getNameAsString();
        el_fn.append_attribute("name") = fn_name.c_str();
        for (auto param : fn->parameters()) {
          auto el_arg = el_fn.append_child("Argument");
          auto param_name = param->getNameAsString();
          el_arg.append_attribute("name") = param_name.c_str();
          auto param_type = param->getType().getAsString();
          el_arg.append_attribute("type") = param_type.c_str();
        }
      }
    }
    doc.save_file(m_exportFileName.c_str());
  }


protected:
  void FoundRecord(clang::CXXRecordDecl const *record) {
    m_fileName = m_sourceman->getFilename(record->getLocation()).str();

    clang::DeclContext const *decl_context =
        record->getEnclosingNamespaceContext();
    clang::NamespaceDecl const *namesp = nullptr;

    if (decl_context->isNamespace()) {
      namesp = clang::cast<clang::NamespaceDecl>(decl_context);
    }

    m_classes.emplace_back(ReflectedClass(namesp, record));
  }

  void FoundField(clang::FieldDecl const *field) {
    m_classes.back().AddField(field);
  }

  void FoundFunction(clang::FunctionDecl const *function) {
    m_classes.back().AddFunction(function);
  }

protected:
  clang::ASTContext *m_context;
  clang::SourceManager *m_sourceman;
  std::vector<ReflectedClass> m_classes;
  std::string m_fileName;
  std::string m_exportFileName;
};

#endif // LLVM_CLANG_TOOLS_EXTRA_METAPP_TOOLING_CLASSFINDER_HPP
