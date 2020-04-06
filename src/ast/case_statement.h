// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_AST_CASE_STATEMENT_H_
#define SRC_AST_CASE_STATEMENT_H_

#include <memory>
#include <utility>
#include <vector>

#include "src/ast/expression.h"
#include "src/ast/literal.h"
#include "src/ast/statement.h"
#include "src/ast/statement_condition.h"

namespace tint {
namespace ast {

/// A case statement
class CaseStatement : public Statement {
 public:
  /// Constructor
  CaseStatement();
  /// Constructor
  /// @param condition the case condition
  /// @param body the case body
  CaseStatement(std::unique_ptr<Literal> condition, StatementList body);
  /// Constructor
  /// @param source the source information
  /// @param condition the case condition
  /// @param body the case body
  CaseStatement(const Source& source,
                std::unique_ptr<Literal> condition,
                StatementList body);
  /// Move constructor
  CaseStatement(CaseStatement&&) = default;
  ~CaseStatement() override;

  /// Sets the condition for the case statement
  /// @param condition the condition to set
  void set_condition(std::unique_ptr<Literal> condition) {
    condition_ = std::move(condition);
  }
  /// @returns the case condition or nullptr if none set
  Literal* condition() const { return condition_.get(); }
  /// @returns true if this is a default statement
  bool IsDefault() const { return condition_ == nullptr; }

  /// Sets the case body
  /// @param body the case body
  void set_body(StatementList body) { body_ = std::move(body); }
  /// @returns the case body
  const StatementList& body() const { return body_; }

  /// @returns true if this is a case statement
  bool IsCase() const override { return true; }

  /// @returns true if the node is valid
  bool IsValid() const override;

  /// Writes a representation of the node to the output stream
  /// @param out the stream to write to
  /// @param indent number of spaces to indent the node when writing
  void to_str(std::ostream& out, size_t indent) const override;

 private:
  CaseStatement(const CaseStatement&) = delete;

  std::unique_ptr<Literal> condition_;
  StatementList body_;
};

/// A list of unique case statements
using CaseStatementList = std::vector<std::unique_ptr<CaseStatement>>;

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_CASE_STATEMENT_H_
