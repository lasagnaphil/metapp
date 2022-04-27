#ifndef LLVM_CLANG_TOOLS_EXTRA_METAPP_TOOLING_CLASSFINDER_HPP
#define LLVM_CLANG_TOOLS_EXTRA_METAPP_TOOLING_CLASSFINDER_HPP
#pragma once

#include "generator.hpp"
#include "generator_store.hpp"
#include "reflectedclass.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>
using namespace nlohmann;

#include <inja/inja.hpp>

#include <fstream>

inline std::vector<std::string> split_string_clean(const std::string& s, char delim) {
  int start = 0;
  std::vector<std::string> tokens;
  for (int i = 0; i <= s.size(); i++) {
    if (i == s.size() || s[i] == delim) {
      int k = start;
      while (s[k] == ' ') k++;
      int l = i-1;
      while (s[l] == ' ') l--;
      tokens.push_back(s.substr(k, l-k+1));
      start = ++i;
    }
  }
  return tokens;
}

inline std::string get_annotation_metadata(clang::Decl::attr_range range) {
  for (auto attr : range) {
    if (attr->getKind() == clang::attr::Annotate) {
      SmallString<64> str;
      raw_svector_ostream os(str);
      clang::LangOptions langopts;
      clang::PrintingPolicy policy(langopts);
      attr->printPretty(os, policy);
      StringRef annotation = str.slice(26, str.size() - 4);
      return annotation.split(';').second.str();
    }
  }
  return "";
}

class ClassFinder : public MatchFinder::MatchCallback {
public:
  // ClassFinder() = default;
  ClassFinder(std::string fileName, std::vector<std::string> tmplNames)
      : m_exportFileName{std::move(fileName)},
        m_tmplFileNames{std::move(tmplNames)} {}

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
    json el_db = json::object();
    for (auto &ref : m_classes) {
      json el_class = json::object();
      auto class_name = ref.m_record->getNameAsString();
      if (ref.m_namespace) {
        el_class["namespace"] = ref.m_namespace->getNameAsString();
      }
      auto attrs_str = get_annotation_metadata(ref.m_record->attrs());
      el_class["attrs"] = split_string_clean(attrs_str, ',');
      for (auto& base : ref.m_record->bases()) {
        el_class["base"] = base.getType().getTypePtr()->getAsRecordDecl()->getNameAsString();
        break; // Assume there is only one base class
      }
      el_class["children"] = json::array();
      el_class["descendants"] = json::array();
      json el_fields = json::array();
      for (auto& field : ref.m_fields) {
        json el_field = json::object();
        el_field["name"] = field->getNameAsString();
        el_field["type"] = field->getType().getAsString();
        el_field["attrs"] = get_annotation_metadata(field->attrs());
        el_fields.push_back(el_field);
      }
      el_class["fields"] = el_fields;
      json el_functions = json::array();
      for (auto& fn : ref.m_functions) {
        json el_fn = json::object();
        el_fn["name"] = fn->getNameAsString();
        json el_params = json::array();
        for (auto& param : fn->parameters()) {
          json el_param = json::object();
          el_param["name"] = param->getNameAsString();
          el_param["type"] = param->getType().getAsString();
          el_params.push_back(el_param);
        }
        el_fn["params"] = el_params;
        el_fn["attrs"] = get_annotation_metadata(fn->attrs());
        el_functions.push_back(el_fn);
      }
      el_class["functions"] = el_functions;

      el_db[class_name] = el_class;
    }

    std::ofstream ofs(m_exportFileName);
    ofs << el_db.dump(4);

    json data;
    data["class_db"] = el_db;

    inja::Environment env;
    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);
    for (const auto& tmplFileName : m_tmplFileNames) {
      auto res = env.render_file(tmplFileName, data);
      auto outputFileName = tmplFileName;
      auto ext_loc = outputFileName.rfind('.');
      outputFileName.erase(ext_loc, outputFileName.size() - ext_loc);
      std::cerr << "Generating " << outputFileName << " from " << tmplFileName
                << "..." << std::endl;
      std::ofstream ofs(outputFileName);
      ofs << res;
    }
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
  std::vector<std::string> m_tmplFileNames;
};

#endif // LLVM_CLANG_TOOLS_EXTRA_METAPP_TOOLING_CLASSFINDER_HPP
