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

#include "src/writer/wgsl/generator_impl.h"

#include <cassert>
#include <limits>

#include "src/ast/array_accessor_expression.h"
#include "src/ast/as_expression.h"
#include "src/ast/assignment_statement.h"
#include "src/ast/binding_decoration.h"
#include "src/ast/bool_literal.h"
#include "src/ast/break_statement.h"
#include "src/ast/builtin_decoration.h"
#include "src/ast/call_expression.h"
#include "src/ast/case_statement.h"
#include "src/ast/cast_expression.h"
#include "src/ast/constructor_expression.h"
#include "src/ast/continue_statement.h"
#include "src/ast/decorated_variable.h"
#include "src/ast/else_statement.h"
#include "src/ast/float_literal.h"
#include "src/ast/identifier_expression.h"
#include "src/ast/if_statement.h"
#include "src/ast/int_literal.h"
#include "src/ast/location_decoration.h"
#include "src/ast/loop_statement.h"
#include "src/ast/member_accessor_expression.h"
#include "src/ast/regardless_statement.h"
#include "src/ast/relational_expression.h"
#include "src/ast/return_statement.h"
#include "src/ast/scalar_constructor_expression.h"
#include "src/ast/set_decoration.h"
#include "src/ast/statement.h"
#include "src/ast/struct.h"
#include "src/ast/struct_member.h"
#include "src/ast/struct_member_offset_decoration.h"
#include "src/ast/switch_statement.h"
#include "src/ast/type/array_type.h"
#include "src/ast/type/matrix_type.h"
#include "src/ast/type/pointer_type.h"
#include "src/ast/type/struct_type.h"
#include "src/ast/type/vector_type.h"
#include "src/ast/type_constructor_expression.h"
#include "src/ast/uint_literal.h"
#include "src/ast/unary_derivative_expression.h"
#include "src/ast/unary_method_expression.h"
#include "src/ast/unary_op_expression.h"
#include "src/ast/unless_statement.h"
#include "src/ast/variable_decl_statement.h"

namespace tint {
namespace writer {
namespace wgsl {

GeneratorImpl::GeneratorImpl() = default;

GeneratorImpl::~GeneratorImpl() = default;

bool GeneratorImpl::Generate(const ast::Module& module) {
  for (const auto& import : module.imports()) {
    if (!EmitImport(import.get())) {
      return false;
    }
  }
  if (!module.imports().empty()) {
    out_ << std::endl;
  }

  for (const auto& ep : module.entry_points()) {
    if (!EmitEntryPoint(ep.get())) {
      return false;
    }
  }
  if (!module.entry_points().empty())
    out_ << std::endl;

  for (const auto& alias : module.alias_types()) {
    if (!EmitAliasType(alias)) {
      return false;
    }
  }
  if (!module.alias_types().empty())
    out_ << std::endl;

  for (const auto& var : module.global_variables()) {
    if (!EmitVariable(var.get())) {
      return false;
    }
  }
  if (!module.global_variables().empty()) {
    out_ << std::endl;
  }

  for (const auto& func : module.functions()) {
    if (!EmitFunction(func.get())) {
      return false;
    }
    out_ << std::endl;
  }

  return true;
}

void GeneratorImpl::make_indent() {
  for (size_t i = 0; i < indent_; i++) {
    out_ << " ";
  }
}

bool GeneratorImpl::EmitAliasType(const ast::type::AliasType* alias) {
  make_indent();
  out_ << "type " << alias->name() << " = ";
  if (!EmitType(alias->type())) {
    return false;
  }
  out_ << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitEntryPoint(const ast::EntryPoint* ep) {
  make_indent();
  out_ << "entry_point " << ep->stage() << " ";
  if (!ep->name().empty() && ep->name() != ep->function_name()) {
    out_ << R"(as ")" << ep->name() << R"(" )";
  }
  out_ << "= " << ep->function_name() << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitExpression(ast::Expression* expr) {
  if (expr->IsArrayAccessor()) {
    return EmitArrayAccessor(expr->AsArrayAccessor());
  }
  if (expr->IsAs()) {
    return EmitAs(expr->AsAs());
  }
  if (expr->IsCall()) {
    return EmitCall(expr->AsCall());
  }
  if (expr->IsCast()) {
    return EmitCast(expr->AsCast());
  }
  if (expr->IsIdentifier()) {
    return EmitIdentifier(expr->AsIdentifier());
  }
  if (expr->IsConstructor()) {
    return EmitConstructor(expr->AsConstructor());
  }
  if (expr->IsMemberAccessor()) {
    return EmitMemberAccessor(expr->AsMemberAccessor());
  }
  if (expr->IsRelational()) {
    return EmitRelational(expr->AsRelational());
  }
  if (expr->IsUnaryDerivative()) {
    return EmitUnaryDerivative(expr->AsUnaryDerivative());
  }
  if (expr->IsUnaryMethod()) {
    return EmitUnaryMethod(expr->AsUnaryMethod());
  }
  if (expr->IsUnaryOp()) {
    return EmitUnaryOp(expr->AsUnaryOp());
  }

  error_ = "unknown expression type";
  return false;
}

bool GeneratorImpl::EmitArrayAccessor(ast::ArrayAccessorExpression* expr) {
  if (!EmitExpression(expr->array())) {
    return false;
  }
  out_ << "[";

  if (!EmitExpression(expr->idx_expr())) {
    return false;
  }
  out_ << "]";

  return true;
}

bool GeneratorImpl::EmitMemberAccessor(ast::MemberAccessorExpression* expr) {
  if (!EmitExpression(expr->structure())) {
    return false;
  }

  out_ << ".";

  return EmitExpression(expr->member());
}

bool GeneratorImpl::EmitAs(ast::AsExpression* expr) {
  out_ << "as<";
  if (!EmitType(expr->type())) {
    return false;
  }

  out_ << ">(";
  if (!EmitExpression(expr->expr())) {
    return false;
  }

  out_ << ")";
  return true;
}

bool GeneratorImpl::EmitCall(ast::CallExpression* expr) {
  if (!EmitExpression(expr->func())) {
    return false;
  }
  out_ << "(";

  bool first = true;
  const auto& params = expr->params();
  for (const auto& param : params) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    if (!EmitExpression(param.get())) {
      return false;
    }
  }

  out_ << ")";

  return true;
}

bool GeneratorImpl::EmitCast(ast::CastExpression* expr) {
  out_ << "cast<";
  if (!EmitType(expr->type())) {
    return false;
  }

  out_ << ">(";
  if (!EmitExpression(expr->expr())) {
    return false;
  }

  out_ << ")";
  return true;
}

bool GeneratorImpl::EmitConstructor(ast::ConstructorExpression* expr) {
  if (expr->IsScalarConstructor()) {
    return EmitScalarConstructor(expr->AsScalarConstructor());
  }
  return EmitTypeConstructor(expr->AsTypeConstructor());
}

bool GeneratorImpl::EmitTypeConstructor(ast::TypeConstructorExpression* expr) {
  if (!EmitType(expr->type())) {
    return false;
  }

  out_ << "(";

  bool first = true;
  for (const auto& e : expr->values()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    if (!EmitExpression(e.get())) {
      return false;
    }
  }

  out_ << ")";
  return true;
}

bool GeneratorImpl::EmitScalarConstructor(
    ast::ScalarConstructorExpression* expr) {
  return EmitLiteral(expr->literal());
}

bool GeneratorImpl::EmitLiteral(ast::Literal* lit) {
  if (lit->IsBool()) {
    out_ << (lit->AsBool()->IsTrue() ? "true" : "false");
  } else if (lit->IsFloat()) {
    auto flags = out_.flags();
    auto precision = out_.precision();

    out_.flags(flags | std::ios_base::showpoint);
    out_.precision(std::numeric_limits<float>::max_digits10);

    out_ << lit->AsFloat()->value();

    out_.precision(precision);
    out_.flags(flags);
  } else if (lit->IsInt()) {
    out_ << lit->AsInt()->value();
  } else if (lit->IsUint()) {
    out_ << lit->AsUint()->value() << "u";
  } else {
    error_ = "unknown literal type";
    return false;
  }
  return true;
}

bool GeneratorImpl::EmitIdentifier(ast::IdentifierExpression* expr) {
  bool first = true;
  for (const auto& part : expr->AsIdentifier()->name()) {
    if (!first) {
      out_ << "::";
    }
    first = false;
    out_ << part;
  }
  return true;
}

bool GeneratorImpl::EmitImport(const ast::Import* import) {
  make_indent();
  out_ << R"(import ")" << import->path() << R"(" as )" << import->name() << ";"
       << std::endl;
  return true;
}

bool GeneratorImpl::EmitFunction(ast::Function* func) {
  make_indent();

  out_ << "fn " << func->name() << "(";

  bool first = true;
  for (const auto& v : func->params()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    out_ << v->name() << " : ";

    if (!EmitType(v->type())) {
      return false;
    }
  }

  out_ << ") -> ";

  if (!EmitType(func->return_type())) {
    return false;
  }

  return EmitStatementBlockAndNewline(func->body());
}

bool GeneratorImpl::EmitType(ast::type::Type* type) {
  if (type->IsAlias()) {
    auto alias = type->AsAlias();
    out_ << alias->name();
  } else if (type->IsArray()) {
    auto ary = type->AsArray();
    out_ << "array<";
    if (!EmitType(ary->type())) {
      return false;
    }

    if (!ary->IsRuntimeArray())
      out_ << ", " << ary->size();

    out_ << ">";
  } else if (type->IsBool()) {
    out_ << "bool";
  } else if (type->IsF32()) {
    out_ << "f32";
  } else if (type->IsI32()) {
    out_ << "i32";
  } else if (type->IsMatrix()) {
    auto mat = type->AsMatrix();
    out_ << "mat" << mat->columns() << "x" << mat->rows() << "<";
    if (!EmitType(mat->type())) {
      return false;
    }
    out_ << ">";
  } else if (type->IsPointer()) {
    auto ptr = type->AsPointer();
    out_ << "ptr<" << ptr->storage_class() << ", ";
    if (!EmitType(ptr->type())) {
      return false;
    }
    out_ << ">";
  } else if (type->IsStruct()) {
    auto str = type->AsStruct()->impl();
    if (str->decoration() != ast::StructDecoration::kNone) {
      out_ << "[[" << str->decoration() << "]] ";
    }
    out_ << "struct {" << std::endl;

    increment_indent();
    for (const auto& mem : str->members()) {
      make_indent();
      if (!mem->decorations().empty()) {
        out_ << "[[";
        bool first = true;
        for (const auto& deco : mem->decorations()) {
          if (!first) {
            out_ << ", ";
          }

          first = false;
          // TODO(dsinclair): Split this out when we have more then one
          assert(deco->IsOffset());

          out_ << "offset " << deco->AsOffset()->offset();
        }
        out_ << "]] ";
      }

      out_ << mem->name() << " : ";
      if (!EmitType(mem->type())) {
        return false;
      }
      out_ << ";" << std::endl;
    }
    decrement_indent();
    make_indent();

    out_ << "}";
  } else if (type->IsU32()) {
    out_ << "u32";
  } else if (type->IsVector()) {
    auto vec = type->AsVector();
    out_ << "vec" << vec->size() << "<";
    if (!EmitType(vec->type())) {
      return false;
    }
    out_ << ">";
  } else if (type->IsVoid()) {
    out_ << "void";
  } else {
    error_ = "unknown type in EmitType";
    return false;
  }

  return true;
}

bool GeneratorImpl::EmitVariable(ast::Variable* var) {
  make_indent();

  if (var->IsDecorated()) {
    if (!EmitVariableDecorations(var->AsDecorated())) {
      return false;
    }
  }

  if (var->is_const()) {
    out_ << "const";
  } else {
    out_ << "var";
    if (var->storage_class() != ast::StorageClass::kNone) {
      out_ << "<" << var->storage_class() << ">";
    }
  }

  out_ << " " << var->name() << " : ";
  if (!EmitType(var->type())) {
    return false;
  }

  if (var->constructor() != nullptr) {
    out_ << " = ";
    if (!EmitExpression(var->constructor())) {
      return false;
    }
  }
  out_ << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitVariableDecorations(ast::DecoratedVariable* var) {
  out_ << "[[";
  bool first = true;
  for (const auto& deco : var->decorations()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    if (deco->IsBinding()) {
      out_ << "binding " << deco->AsBinding()->value();
    } else if (deco->IsSet()) {
      out_ << "set " << deco->AsSet()->value();
    } else if (deco->IsLocation()) {
      out_ << "location " << deco->AsLocation()->value();
    } else if (deco->IsBuiltin()) {
      out_ << "builtin " << deco->AsBuiltin()->value();
    } else {
      error_ = "unknown variable decoration";
      return false;
    }
  }
  out_ << "]] ";

  return true;
}

bool GeneratorImpl::EmitRelational(ast::RelationalExpression* expr) {
  out_ << "(";

  if (!EmitExpression(expr->lhs())) {
    return false;
  }
  out_ << " ";

  switch (expr->relation()) {
    case ast::Relation::kAnd:
      out_ << "&";
      break;
    case ast::Relation::kOr:
      out_ << "|";
      break;
    case ast::Relation::kXor:
      out_ << "^";
      break;
    case ast::Relation::kLogicalAnd:
      out_ << "&&";
      break;
    case ast::Relation::kLogicalOr:
      out_ << "||";
      break;
    case ast::Relation::kEqual:
      out_ << "==";
      break;
    case ast::Relation::kNotEqual:
      out_ << "!=";
      break;
    case ast::Relation::kLessThan:
      out_ << "<";
      break;
    case ast::Relation::kGreaterThan:
      out_ << ">";
      break;
    case ast::Relation::kLessThanEqual:
      out_ << "<=";
      break;
    case ast::Relation::kGreaterThanEqual:
      out_ << ">=";
      break;
    case ast::Relation::kShiftLeft:
      out_ << "<<";
      break;
    case ast::Relation::kShiftRight:
      out_ << ">>";
      break;
    case ast::Relation::kShiftRightArith:
      out_ << ">>>";
      break;
    case ast::Relation::kAdd:
      out_ << "+";
      break;
    case ast::Relation::kSubtract:
      out_ << "-";
      break;
    case ast::Relation::kMultiply:
      out_ << "*";
      break;
    case ast::Relation::kDivide:
      out_ << "/";
      break;
    case ast::Relation::kModulo:
      out_ << "%";
      break;
    case ast::Relation::kNone:
      error_ = "missing relation type";
      return false;
  }
  out_ << " ";

  if (!EmitExpression(expr->rhs())) {
    return false;
  }

  out_ << ")";
  return true;
}

bool GeneratorImpl::EmitUnaryDerivative(ast::UnaryDerivativeExpression* expr) {
  switch (expr->op()) {
    case ast::UnaryDerivative::kDpdx:
      out_ << "dpdx";
      break;
    case ast::UnaryDerivative::kDpdy:
      out_ << "dpdy";
      break;
    case ast::UnaryDerivative::kFwidth:
      out_ << "fwidth";
      break;
  }

  if (expr->modifier() != ast::DerivativeModifier::kNone) {
    out_ << "<" << expr->modifier() << ">";
  }

  out_ << "(";

  if (!EmitExpression(expr->param())) {
    return false;
  }

  out_ << ")";
  return true;
}

bool GeneratorImpl::EmitUnaryMethod(ast::UnaryMethodExpression* expr) {
  switch (expr->op()) {
    case ast::UnaryMethod::kAny:
      out_ << "any";
      break;
    case ast::UnaryMethod::kAll:
      out_ << "all";
      break;
    case ast::UnaryMethod::kIsNan:
      out_ << "is_nan";
      break;
    case ast::UnaryMethod::kIsInf:
      out_ << "is_inf";
      break;
    case ast::UnaryMethod::kIsFinite:
      out_ << "is_finite";
      break;
    case ast::UnaryMethod::kIsNormal:
      out_ << "is_normal";
      break;
    case ast::UnaryMethod::kDot:
      out_ << "dot";
      break;
    case ast::UnaryMethod::kOuterProduct:
      out_ << "outer_product";
      break;
  }
  out_ << "(";

  bool first = true;
  for (const auto& param : expr->params()) {
    if (!first) {
      out_ << ", ";
    }
    first = false;

    if (!EmitExpression(param.get())) {
      return false;
    }
  }
  out_ << ")";

  return true;
}

bool GeneratorImpl::EmitUnaryOp(ast::UnaryOpExpression* expr) {
  switch (expr->op()) {
    case ast::UnaryOp::kNot:
      out_ << "!";
      break;
    case ast::UnaryOp::kNegation:
      out_ << "-";
      break;
  }
  out_ << "(";

  if (!EmitExpression(expr->expr())) {
    return false;
  }

  out_ << ")";

  return true;
}

bool GeneratorImpl::EmitStatementBlock(const ast::StatementList& statements) {
  out_ << " {" << std::endl;

  increment_indent();

  for (const auto& s : statements) {
    if (!EmitStatement(s.get())) {
      return false;
    }
  }

  decrement_indent();
  make_indent();
  out_ << "}";

  return true;
}

bool GeneratorImpl::EmitStatementBlockAndNewline(
    const ast::StatementList& statements) {
  const bool result = EmitStatementBlock(statements);
  if (result) {
    out_ << std::endl;
  }
  return result;
}

bool GeneratorImpl::EmitStatement(ast::Statement* stmt) {
  if (stmt->IsAssign()) {
    return EmitAssign(stmt->AsAssign());
  }
  if (stmt->IsBreak()) {
    return EmitBreak(stmt->AsBreak());
  }
  if (stmt->IsContinue()) {
    return EmitContinue(stmt->AsContinue());
  }
  if (stmt->IsFallthrough()) {
    return EmitFallthrough(stmt->AsFallthrough());
  }
  if (stmt->IsIf()) {
    return EmitIf(stmt->AsIf());
  }
  if (stmt->IsKill()) {
    return EmitKill(stmt->AsKill());
  }
  if (stmt->IsLoop()) {
    return EmitLoop(stmt->AsLoop());
  }
  if (stmt->IsNop()) {
    return EmitNop(stmt->AsNop());
  }
  if (stmt->IsRegardless()) {
    return EmitRegardless(stmt->AsRegardless());
  }
  if (stmt->IsReturn()) {
    return EmitReturn(stmt->AsReturn());
  }
  if (stmt->IsSwitch()) {
    return EmitSwitch(stmt->AsSwitch());
  }
  if (stmt->IsVariableDecl()) {
    return EmitVariable(stmt->AsVariableDecl()->variable());
  }
  if (stmt->IsUnless()) {
    return EmitUnless(stmt->AsUnless());
  }

  error_ = "unknown statement type";
  return false;
}

bool GeneratorImpl::EmitAssign(ast::AssignmentStatement* stmt) {
  make_indent();

  if (!EmitExpression(stmt->lhs())) {
    return false;
  }

  out_ << " = ";

  if (!EmitExpression(stmt->rhs())) {
    return false;
  }

  out_ << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitBreak(ast::BreakStatement* stmt) {
  make_indent();

  out_ << "break";

  if (stmt->condition() != ast::StatementCondition::kNone) {
    out_ << " ";
    if (stmt->condition() == ast::StatementCondition::kIf) {
      out_ << "if";
    } else {
      out_ << "unless";
    }

    out_ << " (";
    if (!EmitExpression(stmt->conditional())) {
      return false;
    }
    out_ << ")";
  }

  out_ << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitCase(ast::CaseStatement* stmt) {
  make_indent();

  if (stmt->IsDefault()) {
    out_ << "default:";
  } else {
    out_ << "case ";

    if (!EmitLiteral(stmt->condition())) {
      return false;
    }
    out_ << ":";
  }

  return EmitStatementBlockAndNewline(stmt->body());
}

bool GeneratorImpl::EmitContinue(ast::ContinueStatement* stmt) {
  make_indent();

  out_ << "continue";

  if (stmt->condition() != ast::StatementCondition::kNone) {
    out_ << " ";
    if (stmt->condition() == ast::StatementCondition::kIf) {
      out_ << "if";
    } else {
      out_ << "unless";
    }

    out_ << " (";
    if (!EmitExpression(stmt->conditional())) {
      return false;
    }
    out_ << ")";
  }

  out_ << ";" << std::endl;

  return true;
}

bool GeneratorImpl::EmitElse(ast::ElseStatement* stmt) {
  if (stmt->HasCondition()) {
    out_ << " elseif (";
    if (!EmitExpression(stmt->condition())) {
      return false;
    }
    out_ << ")";
  } else {
    out_ << " else";
  }

  return EmitStatementBlock(stmt->body());
}

bool GeneratorImpl::EmitFallthrough(ast::FallthroughStatement*) {
  make_indent();
  out_ << "fallthrough;" << std::endl;
  return true;
}

bool GeneratorImpl::EmitIf(ast::IfStatement* stmt) {
  make_indent();

  out_ << "if (";
  if (!EmitExpression(stmt->condition())) {
    return false;
  }
  out_ << ")";

  if (!EmitStatementBlock(stmt->body())) {
    return false;
  }

  for (const auto& e : stmt->else_statements()) {
    if (!EmitElse(e.get())) {
      return false;
    }
  }
  out_ << std::endl;

  return true;
}

bool GeneratorImpl::EmitKill(ast::KillStatement*) {
  make_indent();
  out_ << "kill;" << std::endl;
  return true;
}

bool GeneratorImpl::EmitLoop(ast::LoopStatement* stmt) {
  make_indent();

  out_ << "loop {" << std::endl;
  increment_indent();

  for (const auto& s : stmt->body()) {
    if (!EmitStatement(s.get())) {
      return false;
    }
  }

  if (stmt->has_continuing()) {
    out_ << std::endl;

    make_indent();
    out_ << "continuing";

    if (!EmitStatementBlockAndNewline(stmt->continuing())) {
      return false;
    }
  }

  decrement_indent();
  make_indent();
  out_ << "}" << std::endl;

  return true;
}

bool GeneratorImpl::EmitNop(ast::NopStatement*) {
  make_indent();
  out_ << "nop;" << std::endl;
  return true;
}

bool GeneratorImpl::EmitRegardless(ast::RegardlessStatement* stmt) {
  make_indent();

  out_ << "regardless (";
  if (!EmitExpression(stmt->condition())) {
    return false;
  }
  out_ << ")";

  return EmitStatementBlockAndNewline(stmt->body());
}

bool GeneratorImpl::EmitReturn(ast::ReturnStatement* stmt) {
  make_indent();

  out_ << "return";
  if (stmt->has_value()) {
    out_ << " ";
    if (!EmitExpression(stmt->value())) {
      return false;
    }
  }
  out_ << ";" << std::endl;
  return true;
}

bool GeneratorImpl::EmitSwitch(ast::SwitchStatement* stmt) {
  make_indent();

  out_ << "switch(";
  if (!EmitExpression(stmt->condition())) {
    return false;
  }
  out_ << ") {" << std::endl;

  increment_indent();

  for (const auto& s : stmt->body()) {
    if (!EmitCase(s.get())) {
      return false;
    }
  }

  decrement_indent();
  make_indent();
  out_ << "}" << std::endl;

  return true;
}

bool GeneratorImpl::EmitUnless(ast::UnlessStatement* stmt) {
  make_indent();

  out_ << "unless (";
  if (!EmitExpression(stmt->condition())) {
    return false;
  }
  out_ << ")";

  return EmitStatementBlockAndNewline(stmt->body());
}

}  // namespace wgsl
}  // namespace writer
}  // namespace tint
