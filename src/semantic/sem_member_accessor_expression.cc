// Copyright 2021 The Tint Authors.
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

#include "src/semantic/member_accessor_expression.h"

TINT_INSTANTIATE_CLASS_ID(tint::semantic::MemberAccessorExpression);

namespace tint {
namespace semantic {

MemberAccessorExpression::MemberAccessorExpression(type::Type* type,
                                                   Statement* statement,
                                                   bool is_swizzle)
    : Base(type, statement), is_swizzle_(is_swizzle) {}

}  // namespace semantic
}  // namespace tint
