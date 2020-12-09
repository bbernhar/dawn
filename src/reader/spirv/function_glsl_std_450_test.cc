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

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "src/reader/spirv/fail_stream.h"
#include "src/reader/spirv/function.h"
#include "src/reader/spirv/parser_impl.h"
#include "src/reader/spirv/parser_impl_test_helper.h"
#include "src/reader/spirv/spirv_tools_helpers_test.h"

namespace tint {
namespace reader {
namespace spirv {
namespace {

using ::testing::HasSubstr;

std::string Preamble() {
  return R"(
  OpCapability Shader
  %glsl = OpExtInstImport "GLSL.std.450"
  OpMemoryModel Logical GLSL450
  OpEntryPoint GLCompute %100 "main"
  OpExecutionMode %100 LocalSize 1 1 1

  OpName %u1 "u1"
  OpName %u2 "u2"
  OpName %u3 "u3"
  OpName %i1 "i1"
  OpName %i2 "i2"
  OpName %i3 "i3"
  OpName %f1 "f1"
  OpName %f2 "f2"
  OpName %f3 "f3"
  OpName %v2u1 "v2u1"
  OpName %v2u2 "v2u2"
  OpName %v2u3 "v2u3"
  OpName %v2i1 "v2i1"
  OpName %v2i2 "v2i2"
  OpName %v2i3 "v2i3"
  OpName %v2f1 "v2f1"
  OpName %v2f2 "v2f2"
  OpName %v2f3 "v2f3"
  OpName %v3f1 "v3f1"
  OpName %v3f2 "v3f2"

  %void = OpTypeVoid
  %voidfn = OpTypeFunction %void

  %uint = OpTypeInt 32 0
  %int = OpTypeInt 32 1
  %float = OpTypeFloat 32

  %uint_10 = OpConstant %uint 10
  %uint_15 = OpConstant %uint 15
  %uint_20 = OpConstant %uint 20
  %int_30 = OpConstant %int 30
  %int_35 = OpConstant %int 35
  %int_40 = OpConstant %int 40
  %float_50 = OpConstant %float 50
  %float_60 = OpConstant %float 60
  %float_70 = OpConstant %float 70

  %v2uint = OpTypeVector %uint 2
  %v2int = OpTypeVector %int 2
  %v2float = OpTypeVector %float 2
  %v3float = OpTypeVector %float 3

  %v2uint_10_20 = OpConstantComposite %v2uint %uint_10 %uint_20
  %v2uint_20_10 = OpConstantComposite %v2uint %uint_20 %uint_10
  %v2uint_15_15 = OpConstantComposite %v2uint %uint_15 %uint_15
  %v2int_30_40 = OpConstantComposite %v2int %int_30 %int_40
  %v2int_40_30 = OpConstantComposite %v2int %int_40 %int_30
  %v2int_35_35 = OpConstantComposite %v2int %int_35 %int_35
  %v2float_50_60 = OpConstantComposite %v2float %float_50 %float_60
  %v2float_60_50 = OpConstantComposite %v2float %float_60 %float_50
  %v2float_70_70 = OpConstantComposite %v2float %float_70 %float_70

  %v3float_50_60_70 = OpConstantComposite %v3float %float_50 %float_60 %float_70
  %v3float_60_70_50 = OpConstantComposite %v3float %float_60 %float_70 %float_50

  %100 = OpFunction %void None %voidfn
  %entry = OpLabel

  %u1 = OpCopyObject %uint %uint_10
  %u2 = OpCopyObject %uint %uint_15
  %u3 = OpCopyObject %uint %uint_20

  %i1 = OpCopyObject %int %int_30
  %i2 = OpCopyObject %int %int_35
  %i3 = OpCopyObject %int %int_40

  %f1 = OpCopyObject %float %float_50
  %f2 = OpCopyObject %float %float_60
  %f3 = OpCopyObject %float %float_70

  %v2u1 = OpCopyObject %v2uint %v2uint_10_20
  %v2u2 = OpCopyObject %v2uint %v2uint_20_10
  %v2u3 = OpCopyObject %v2uint %v2uint_15_15

  %v2i1 = OpCopyObject %v2int %v2int_30_40
  %v2i2 = OpCopyObject %v2int %v2int_40_30
  %v2i3 = OpCopyObject %v2int %v2int_35_35

  %v2f1 = OpCopyObject %v2float %v2float_50_60
  %v2f2 = OpCopyObject %v2float %v2float_60_50
  %v2f3 = OpCopyObject %v2float %v2float_70_70

  %v3f1 = OpCopyObject %v3float %v3float_50_60_70
  %v3f2 = OpCopyObject %v3float %v3float_60_70_50

)";
}

struct GlslStd450Case {
  std::string opcode;
  std::string wgsl_func;
};
inline std::ostream& operator<<(std::ostream& out, GlslStd450Case c) {
  out << "GlslStd450Case(" << c.opcode << " " << c.wgsl_func << ")";
  return out;
}

// Nomenclature:
// Float = scalar float
// Floating = scalar float or vector-of-float
// Float3 = 3-element vector of float
// Int = scalar signed int
// Inting = scalar int or vector-of-int
// Uint = scalar unsigned int
// Uinting = scalar unsigned or vector-of-unsigned

using SpvParserTest_GlslStd450_Float_Floating =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Float_FloatingFloating =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Floating_Floating =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Floating_FloatingFloating =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Floating_FloatingFloatingFloating =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Floating_FloatingInting =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Floating_FloatingUinting =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Float3_Float3Float3 =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;

using SpvParserTest_GlslStd450_Inting_IntingIntingInting =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;
using SpvParserTest_GlslStd450_Uinting_UintingUintingUinting =
    SpvParserTestBase<::testing::TestWithParam<GlslStd450Case>>;

TEST_P(SpvParserTest_GlslStd450_Float_Floating, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %f1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{f1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Float_Floating, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %v2f1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2f1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Float_FloatingFloating, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %f1 %f2
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{f1}
          Identifier[not set]{f2}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Float_FloatingFloating, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %v2f1 %v2f2
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2f1}
          Identifier[not set]{v2f2}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_Floating, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %f1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{f1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_Floating, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v2float %glsl )" +
                        GetParam().opcode + R"( %v2f1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2f1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingFloating, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %f1 %f2
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{f1}
          Identifier[not set]{f2}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingFloating, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v2float %glsl )" +
                        GetParam().opcode + R"( %v2f1 %v2f2
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2f1}
          Identifier[not set]{v2f2}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingFloatingFloating, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %f1 %f2 %f3
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{f1}
          Identifier[not set]{f2}
          Identifier[not set]{f3}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingFloatingFloating, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v2float %glsl )" +
                        GetParam().opcode +
                        R"( %v2f1 %v2f2 %v2f3
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2f1}
          Identifier[not set]{v2f2}
          Identifier[not set]{v2f3}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingUinting, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %f1 %u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{f1}
          Identifier[not set]{u1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingUinting, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v2float %glsl )" +
                        GetParam().opcode +
                        R"( %v2f1 %v2u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2f1}
          Identifier[not set]{v2u1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingInting, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %float %glsl )" +
                        GetParam().opcode + R"( %f1 %i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{f1}
          Identifier[not set]{i1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Floating_FloatingInting, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v2float %glsl )" +
                        GetParam().opcode +
                        R"( %v2f1 %v2i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2f1}
          Identifier[not set]{v2i1}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Float3_Float3Float3, Samples) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v3float %glsl )" +
                        GetParam().opcode +
                        R"( %v3f1 %v3f2
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_3__f32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v3f1}
          Identifier[not set]{v3f2}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Float_Floating,
                         ::testing::Values(GlslStd450Case{"Length", "length"}));

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Float_FloatingFloating,
                         ::testing::Values(GlslStd450Case{"Distance",
                                                          "distance"}));

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Floating_Floating,
                         ::testing::ValuesIn(std::vector<GlslStd450Case>{
                             {"Acos", "acos"},
                             {"Asin", "asin"},
                             {"Atan", "atan"},
                             {"Ceil", "ceil"},
                             {"Cos", "cos"},
                             {"Cosh", "cosh"},
                             {"Exp", "exp"},
                             {"Exp2", "exp2"},
                             {"FAbs", "abs"},
                             {"FSign", "sign"},
                             {"Floor", "floor"},
                             {"Fract", "fract"},
                             {"InverseSqrt", "inverseSqrt"},
                             {"Log", "log"},
                             {"Log2", "log2"},
                             {"Normalize", "normalize"},
                             {"Round", "round"},
                             {"Sin", "sin"},
                             {"Sinh", "sinh"},
                             {"Sqrt", "sqrt"},
                             {"Tan", "tan"},
                             {"Tanh", "tanh"},
                             {"Trunc", "trunc"},
                         }));

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Floating_FloatingFloating,
                         ::testing::ValuesIn(std::vector<GlslStd450Case>{
                             {"Atan2", "atan2"},
                             {"NMax", "max"},
                             {"NMin", "min"},
                             {"FMax", "max"},  // WGSL max promises more for NaN
                             {"FMin", "min"},  // WGSL min promises more for NaN
                             {"Pow", "pow"},
                             {"Reflect", "reflect"},
                             {"Step", "step"},
                         }));

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Floating_FloatingUinting,
                         ::testing::Values(GlslStd450Case{"Ldexp", "ldexp"}));

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Floating_FloatingInting,
                         ::testing::Values(GlslStd450Case{"Ldexp", "ldexp"}));

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Float3_Float3Float3,
                         ::testing::Values(GlslStd450Case{"Cross", "cross"}));

INSTANTIATE_TEST_SUITE_P(
    Samples,
    SpvParserTest_GlslStd450_Floating_FloatingFloatingFloating,
    ::testing::ValuesIn(std::vector<GlslStd450Case>{
        {"NClamp", "clamp"},
        {"FClamp", "clamp"},  // WGSL FClamp promises more for NaN
        {"FaceForward", "faceForward"},
        {"Fma", "fma"},
        {"FMix", "mix"},
        {"SmoothStep", "smoothStep"}}));

TEST_P(SpvParserTest_GlslStd450_Inting_IntingIntingInting, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %int %glsl )" +
                        GetParam().opcode +
                        R"( %i1 %i2 %i3
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __i32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{i1}
          Identifier[not set]{i2}
          Identifier[not set]{i3}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Inting_IntingIntingInting, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v2int %glsl )" +
                        GetParam().opcode +
                        R"( %v2i1 %v2i2 %v2i3
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__i32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2i1}
          Identifier[not set]{v2i2}
          Identifier[not set]{v2i3}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Inting_IntingIntingInting,
                         ::testing::Values(GlslStd450Case{"SClamp", "clamp"}));

TEST_P(SpvParserTest_GlslStd450_Uinting_UintingUintingUinting, Scalar) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %uint %glsl )" +
                        GetParam().opcode + R"( %u1 %u2 %u3
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{u1}
          Identifier[not set]{u2}
          Identifier[not set]{u3}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

TEST_P(SpvParserTest_GlslStd450_Uinting_UintingUintingUinting, Vector) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %v2uint %glsl )" +
                        GetParam().opcode +
                        R"( %v2u1 %v2u2 %v2u3
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Call[not set]{
        Identifier[not set]{)" + GetParam().wgsl_func +
                                                 R"(}
        (
          Identifier[not set]{v2u1}
          Identifier[not set]{v2u2}
          Identifier[not set]{v2u3}
        )
      }
    }
  })"))
      << ToString(fe.ast_body());
}

INSTANTIATE_TEST_SUITE_P(Samples,
                         SpvParserTest_GlslStd450_Uinting_UintingUintingUinting,
                         ::testing::Values(GlslStd450Case{"UClamp", "clamp"}));

TEST_F(SpvParserTest, RectifyOperandsAndResult_GLSLstd450SClamp) {
  const auto assembly = Preamble() + R"(
     %1 = OpExtInst %uint %glsl SClamp %u1 %i2 %u3
     %2 = OpExtInst %v2uint %glsl SClamp %v2u1 %v2i2 %v2u3
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  auto body = ToString(fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Bitcast[not set]<__u32>{
        Call[not set]{
          Identifier[not set]{clamp}
          (
            Bitcast[not set]<__i32>{
              Identifier[not set]{u1}
            }
            Identifier[not set]{i2}
            Bitcast[not set]<__i32>{
              Identifier[not set]{u3}
            }
          )
        }
      }
    }
  })"))
      << body;
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_2
    none
    __vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Call[not set]{
          Identifier[not set]{clamp}
          (
            Bitcast[not set]<__vec_2__i32>{
              Identifier[not set]{v2u1}
            }
            Identifier[not set]{v2i2}
            Bitcast[not set]<__vec_2__i32>{
              Identifier[not set]{v2u3}
            }
          )
        }
      }
    }
  })"))
      << body;
}

}  // namespace
}  // namespace spirv
}  // namespace reader
}  // namespace tint
