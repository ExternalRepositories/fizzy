// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include "trunc_boundaries.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/floating_point_utils.hpp>
#include <test/utils/hex.hpp>
#include <cmath>

using namespace fizzy;
using namespace fizzy::test;

MATCHER_P(CanonicalNaN, value, "result with a canonical NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return FP<value_type>{result_value}.nan_payload() == FP<value_type>::canon;
}

MATCHER_P(ArithmeticNaN, value, "result with an arithmetic NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return FP<value_type>{result_value}.nan_payload() >= FP<value_type>::canon;
}

TEST(execute_floating_point, f32_const)
{
    /* wat2wasm
    (func (result f32)
      f32.const 4194304.1
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017d030201000a09010700430000804a0b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result(4194304.1f));
}

TEST(execute_floating_point, f64_const)
{
    /* wat2wasm
    (func (result f64)
      f64.const 8589934592.1
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017c030201000a0d010b0044cdcc0000000000420b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result(8589934592.1));
}

TEST(execute_floating_point, f32_add)
{
    /* wat2wasm
    (func (param f32 f32) (result f32)
      local.get 0
      local.get 1
      f32.add
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027d7d017d030201000a0901070020002001920b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {1.001f, 6.006f}), Result(7.007f));
}

TEST(execute_floating_point, f64_add)
{
    /* wat2wasm
    (func (param f64 f64) (result f64)
      local.get 0
      local.get 1
      f64.add
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027c7c017c030201000a0901070020002001a00b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {1.0011, 6.0066}), Result(7.0077));
}

/// Compile-time information about a Wasm type.
template <typename T>
struct WasmTypeTraits;

template <>
struct WasmTypeTraits<float>
{
    static constexpr auto name = "f32";
    static constexpr auto valtype = ValType::f32;
};
template <>
struct WasmTypeTraits<double>
{
    static constexpr auto name = "f64";
    static constexpr auto valtype = ValType::f64;
};

struct WasmTypeName
{
    template <typename T>
    static std::string GetName(int)
    {
        return WasmTypeTraits<T>::name;
    }
};

template <typename T>
class execute_floating_point_types : public testing::Test
{
protected:
    /// Creates a wasm module with a single function for the given instructions opcode.
    /// The opcode is converted to match the type, e.g. f32_add -> f64_add.
    static bytes get_binop_code(Instr opcode)
    {
        constexpr auto f64_variant_offset =
            static_cast<uint8_t>(Instr::f64_add) - static_cast<uint8_t>(Instr::f32_add);

        // Convert to f64 variant if needed.
        const auto typed_opcode =
            std::is_same_v<T, double> ?
                static_cast<uint8_t>(static_cast<uint8_t>(opcode) + f64_variant_offset) :
                static_cast<uint8_t>(opcode);

        /* wat2wasm
        (func (param f32 f32) (result f32)
          (f32.add (local.get 0) (local.get 1))
        )
        */
        auto wasm = from_hex("0061736d0100000001070160027d7d017d030201000a0901070020002001920b");
        constexpr auto template_type = static_cast<uint8_t>(ValType::f32);
        constexpr auto template_opcode = static_cast<uint8_t>(Instr::f32_add);
        EXPECT_EQ(std::count(wasm.begin(), wasm.end(), template_type), 3);
        EXPECT_EQ(std::count(wasm.begin(), wasm.end(), template_opcode), 1);
        std::replace(wasm.begin(), wasm.end(), template_type,
            static_cast<uint8_t>(WasmTypeTraits<T>::valtype));
        std::replace(wasm.begin(), wasm.end(), template_opcode, typed_opcode);
        return wasm;
    }
};

using FloatingPointTypes = testing::Types<float, double>;
TYPED_TEST_SUITE(execute_floating_point_types, FloatingPointTypes, WasmTypeName);

TYPED_TEST(execute_floating_point_types, nan_matchers)
{
    using testing::Not;
    using FP = FP<TypeParam>;

    EXPECT_THAT(Void, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(Trap, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{TypeParam{}}}, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon)}}, CanonicalNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon + 1)}}, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(1)}}, Not(CanonicalNaN(TypeParam{})));

    EXPECT_THAT(Void, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(Trap, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{TypeParam{}}}, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon)}}, ArithmeticNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon + 1)}}, ArithmeticNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(1)}}, Not(ArithmeticNaN(TypeParam{})));
}

TYPED_TEST(execute_floating_point_types, binop_nan_propagation)
{
    // Tests NaN propagation in binary instructions (binop).
    // If all NaN inputs are canonical the result must be the canonical NaN.
    // Otherwise, the result must be an arithmetic NaN.

    // The list of instructions to be tested.
    // Only f32 variants, but f64 variants are going to be covered as well.
    constexpr Instr opcodes[] = {
        Instr::f32_add,
    };

    for (const auto op : opcodes)
    {
        auto instance = instantiate(parse(this->get_binop_code(op)));

        constexpr auto q = TypeParam{1.0};
        const auto cnan = FP<TypeParam>::nan(FP<TypeParam>::canon);
        const auto anan = FP<TypeParam>::nan(FP<TypeParam>::canon + 1);

        EXPECT_THAT(execute(*instance, 0, {q, cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, q}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, cnan}), CanonicalNaN(TypeParam{}));

        EXPECT_THAT(execute(*instance, 0, {q, anan}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {anan, q}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {anan, anan}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {anan, cnan}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, anan}), ArithmeticNaN(TypeParam{}));
    }
}

TYPED_TEST(execute_floating_point_types, compare)
{
    using Limits = typename FP<TypeParam>::Limits;

    /* wat2wasm
    (func (param f32 f32) (result i32) (f32.eq (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.ne (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.lt (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.gt (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.le (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.ge (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.eq (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.ne (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.lt (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.gt (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.le (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.ge (local.get 0) (local.get 1)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010d0260027d7d017f60027c7c017f030d0c0000000000000101010101010a610c07002000"
        "20015b0b0700200020015c0b0700200020015d0b0700200020015e0b0700200020015f0b070020002001600b07"
        "0020002001610b070020002001620b070020002001630b070020002001640b070020002001650b070020002001"
        "660b");
    auto inst = instantiate(parse(wasm));

    constexpr FuncIdx func_offset = std::is_same_v<TypeParam, float> ? 0 : 6;
    constexpr auto eq = func_offset + 0;
    constexpr auto ne = func_offset + 1;
    constexpr auto lt = func_offset + 2;
    constexpr auto gt = func_offset + 3;
    constexpr auto le = func_offset + 4;
    constexpr auto ge = func_offset + 5;

    // The values to be used as test cases.
    // They must be strictly ordered (ordered_values[i] < ordered_values[j] for i<j) or NaNs.
    // This allows determining the relation of any pair of values only knowing values' position
    // in the array.
    const TypeParam ordered_values[] = {
        -Limits::infinity(),
        -Limits::max(),
        std::nextafter(-Limits::max(), TypeParam{0}),
        std::nextafter(-TypeParam{1.0}, -Limits::infinity()),
        -TypeParam{1.0},
        std::nextafter(-TypeParam{1.0}, TypeParam{0}),
        std::nextafter(-Limits::min(), -Limits::infinity()),
        -Limits::min(),
        std::nextafter(-Limits::min(), TypeParam{0}),
        std::nextafter(-Limits::denorm_min(), -Limits::infinity()),
        -Limits::denorm_min(),
        TypeParam{0},
        Limits::denorm_min(),
        std::nextafter(Limits::denorm_min(), Limits::infinity()),
        std::nextafter(Limits::min(), TypeParam{0}),
        Limits::min(),
        std::nextafter(Limits::min(), Limits::infinity()),
        std::nextafter(TypeParam{1.0}, TypeParam{0}),
        TypeParam{1.0},
        std::nextafter(TypeParam{1.0}, Limits::infinity()),
        std::nextafter(Limits::max(), TypeParam{0}),
        Limits::max(),
        Limits::infinity(),

        // NaNs.
        FP<TypeParam>::nan(FP<TypeParam>::canon),
        FP<TypeParam>::nan(FP<TypeParam>::canon + 1),
        FP<TypeParam>::nan(1),
    };

    // Check every pair from cartesian product of ordered_values.
    for (size_t i = 0; i < std::size(ordered_values); ++i)
    {
        for (size_t j = 0; j < std::size(ordered_values); ++j)
        {
            const auto a = ordered_values[i];
            const auto b = ordered_values[j];
            if (std::isnan(a) || std::isnan(b))
            {
                EXPECT_THAT(execute(*inst, eq, {a, b}), Result(0)) << a << "==" << b;
                EXPECT_THAT(execute(*inst, ne, {a, b}), Result(1)) << a << "!=" << b;
                EXPECT_THAT(execute(*inst, lt, {a, b}), Result(0)) << a << "<" << b;
                EXPECT_THAT(execute(*inst, gt, {a, b}), Result(0)) << a << ">" << b;
                EXPECT_THAT(execute(*inst, le, {a, b}), Result(0)) << a << "<=" << b;
                EXPECT_THAT(execute(*inst, ge, {a, b}), Result(0)) << a << ">=" << b;
            }
            else
            {
                EXPECT_THAT(execute(*inst, eq, {a, b}), Result(uint32_t{i == j})) << a << "==" << b;
                EXPECT_THAT(execute(*inst, ne, {a, b}), Result(uint32_t{i != j})) << a << "!=" << b;
                EXPECT_THAT(execute(*inst, lt, {a, b}), Result(uint32_t{i < j})) << a << "<" << b;
                EXPECT_THAT(execute(*inst, gt, {a, b}), Result(uint32_t{i > j})) << a << ">" << b;
                EXPECT_THAT(execute(*inst, le, {a, b}), Result(uint32_t{i <= j})) << a << "<=" << b;
                EXPECT_THAT(execute(*inst, ge, {a, b}), Result(uint32_t{i >= j})) << a << ">=" << b;
            }
        }
    }

    // Negative zero. This is separate set of checks because -0.0 cannot be placed
    // in the ordered_values array as -0.0 == 0.0.
    EXPECT_THAT(execute(*inst, eq, {TypeParam{-0.0}, TypeParam{0.0}}), Result(1));
    EXPECT_THAT(execute(*inst, ne, {TypeParam{-0.0}, TypeParam{0.0}}), Result(0));
    EXPECT_THAT(execute(*inst, lt, {TypeParam{-0.0}, TypeParam{0.0}}), Result(0));
    EXPECT_THAT(execute(*inst, gt, {TypeParam{-0.0}, TypeParam{0.0}}), Result(0));
    EXPECT_THAT(execute(*inst, le, {TypeParam{-0.0}, TypeParam{0.0}}), Result(1));
    EXPECT_THAT(execute(*inst, ge, {TypeParam{-0.0}, TypeParam{0.0}}), Result(1));
}


template <typename SrcT, typename DstT>
struct ConversionPairWasmTraits;

template <>
struct ConversionPairWasmTraits<float, int32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f32_s";
    static constexpr auto opcode = Instr::i32_trunc_f32_s;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<float, uint32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f32_u";
    static constexpr auto opcode = Instr::i32_trunc_f32_u;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<double, int32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f64_s";
    static constexpr auto opcode = Instr::i32_trunc_f64_s;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<double, uint32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f64_u";
    static constexpr auto opcode = Instr::i32_trunc_f64_u;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<float, int64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f32_s";
    static constexpr auto opcode = Instr::i64_trunc_f32_s;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i64;
};
template <>
struct ConversionPairWasmTraits<float, uint64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f32_u";
    static constexpr auto opcode = Instr::i64_trunc_f32_u;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i64;
};
template <>
struct ConversionPairWasmTraits<double, int64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f64_s";
    static constexpr auto opcode = Instr::i64_trunc_f64_s;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i64;
};
template <>
struct ConversionPairWasmTraits<double, uint64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f64_u";
    static constexpr auto opcode = Instr::i64_trunc_f64_u;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i64;
};

template <typename SrcT, typename DstT>
struct ConversionPair : ConversionPairWasmTraits<SrcT, DstT>
{
    using src_type = SrcT;
    using dst_type = DstT;
};

struct ConversionName
{
    template <typename T>
    static std::string GetName(int /*unused*/)
    {
        return T::opcode_name;
    }
};

template <typename T>
class execute_floating_point_trunc : public testing::Test
{
};

using TruncPairs = testing::Types<ConversionPair<float, int32_t>, ConversionPair<float, uint32_t>,
    ConversionPair<double, int32_t>, ConversionPair<double, uint32_t>,
    ConversionPair<float, int64_t>, ConversionPair<float, uint64_t>,
    ConversionPair<double, int64_t>, ConversionPair<double, uint64_t>>;
TYPED_TEST_SUITE(execute_floating_point_trunc, TruncPairs, ConversionName);

TYPED_TEST(execute_floating_point_trunc, trunc)
{
    using FloatT = typename TypeParam::src_type;
    using IntT = typename TypeParam::dst_type;
    using FloatLimits = std::numeric_limits<FloatT>;
    using IntLimits = std::numeric_limits<IntT>;

    /* wat2wasm
    (func (param f32) (result i32)
      local.get 0
      i32.trunc_f32_s
    )
    */
    auto wasm = from_hex("0061736d0100000001060160017d017f030201000a070105002000a80b");

    // Find and replace changeable values: types and the conversion instruction.
    constexpr auto param_type = static_cast<uint8_t>(ValType::f32);
    constexpr auto result_type = static_cast<uint8_t>(ValType::i32);
    constexpr auto opcode = static_cast<uint8_t>(Instr::i32_trunc_f32_s);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), param_type), 1);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), result_type), 1);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), opcode), 1);
    *std::find(wasm.begin(), wasm.end(), param_type) = static_cast<uint8_t>(TypeParam::src_valtype);
    *std::find(wasm.begin(), wasm.end(), result_type) =
        static_cast<uint8_t>(TypeParam::dst_valtype);
    *std::find(wasm.begin(), wasm.end(), opcode) = static_cast<uint8_t>(TypeParam::opcode);

    auto instance = instantiate(parse(wasm));

    // Zero.
    EXPECT_THAT(execute(*instance, 0, {FloatT{0}}), Result(IntT{0}));
    EXPECT_THAT(execute(*instance, 0, {-FloatT{0}}), Result(IntT{0}));

    // Something around 0.0.
    EXPECT_THAT(execute(*instance, 0, {FloatLimits::denorm_min()}), Result(IntT{0}));
    EXPECT_THAT(execute(*instance, 0, {-FloatLimits::denorm_min()}), Result(IntT{0}));

    // Something smaller than 2.0.
    EXPECT_THAT(execute(*instance, 0, {std::nextafter(FloatT{2}, FloatT{0})}), Result(IntT{1}));

    // Something bigger than -1.0.
    EXPECT_THAT(execute(*instance, 0, {std::nextafter(FloatT{-1}, FloatT{0})}), Result(IntT{0}));

    {
        // BOUNDARIES OF DEFINITION
        //
        // Here we want to identify and test the boundary values of the defined behavior of the
        // trunc instructions. For undefined results the execution must trap.
        // Note that floating point type can represent any power of 2.

        using expected_boundaries = trunc_boundaries<FloatT, IntT>;

        // For iN with max value 2^N-1 the float(2^N) exists and trunc(float(2^N)) to iN
        // is undefined.
        const auto upper_boundary = std::pow(FloatT{2}, FloatT{IntLimits::digits});
        EXPECT_EQ(upper_boundary, expected_boundaries::upper);
        EXPECT_THAT(execute(*instance, 0, {upper_boundary}), Traps());

        // But the trunc() of the next float value smaller than 2^N is defined.
        // Depending on the resolution of the floating point type, the result integer value may
        // be other than 2^(N-1).
        const auto max_defined = std::nextafter(upper_boundary, FloatT{0});
        const auto max_defined_int = static_cast<IntT>(max_defined);
        EXPECT_THAT(execute(*instance, 0, {max_defined}), Result(max_defined_int));

        // The lower boundary is:
        // - for signed integers: -2^N - 1,
        // - for unsigned integers: -1.
        // However, the -2^N - 1 may be not representative in a float type so we compute it as
        // floor(-2^N - epsilon).
        const auto min_defined_int = IntLimits::min();
        const auto lower_boundary =
            std::floor(std::nextafter(FloatT{min_defined_int}, -FloatLimits::infinity()));
        EXPECT_EQ(lower_boundary, expected_boundaries::lower);
        EXPECT_THAT(execute(*instance, 0, {lower_boundary}), Traps());

        const auto min_defined = std::nextafter(lower_boundary, FloatT{0});
        EXPECT_THAT(execute(*instance, 0, {min_defined}), Result(min_defined_int));
    }

    {
        // NaNs.
        EXPECT_THAT(execute(*instance, 0, {FloatLimits::quiet_NaN()}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FloatLimits::signaling_NaN()}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(FP<FloatT>::canon)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(FP<FloatT>::canon)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(FP<FloatT>::canon + 1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(FP<FloatT>::canon + 1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(0xdead)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(0xdead)}), Traps());
        const auto signaling_nan = FP<FloatT>::nan(FP<FloatT>::canon >> 1);
        EXPECT_THAT(execute(*instance, 0, {signaling_nan}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-signaling_nan}), Traps());

        const auto inf = FloatLimits::infinity();
        EXPECT_THAT(execute(*instance, 0, {inf}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-inf}), Traps());

        EXPECT_THAT(execute(*instance, 0, {FloatLimits::max()}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FloatLimits::lowest()}), Traps());
    }

    if constexpr (IntLimits::is_signed)
    {
        // Something bigger than -2.0.
        const auto arg = std::nextafter(FloatT{-2}, FloatT{0});
        const auto result = execute(*instance, 0, {arg});
        EXPECT_EQ(result.value.template as<IntT>(), FloatT{-1});
    }
}

TEST(execute_floating_point, f32_load)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f32)
      (f32.load (local.get 0))
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017d030201000504010101010a0901070020002a02000b");

    auto instance = instantiate(parse(wasm));

    const std::tuple<bytes, float> test_cases[]{
        {"00000000"_bytes, 0.0f},
        {"00000080"_bytes, -0.0f},
        {"b6f39d3f"_bytes, 1.234f},
        {"b6f39dbf"_bytes, -1.234f},
        {"0000807f"_bytes, FP32::Limits::infinity()},
        {"000080ff"_bytes, -FP32::Limits::infinity()},
        {"ffff7f7f"_bytes, FP32::Limits::max()},
        {"ffff7fff"_bytes, -FP32::Limits::max()},
        {"00008000"_bytes, FP32::Limits::min()},
        {"00008080"_bytes, -FP32::Limits::min()},
        {"01000000"_bytes, FP32::Limits::denorm_min()},
        {"01000080"_bytes, -FP32::Limits::denorm_min()},
        {"0000803f"_bytes, 1.0f},
        {"000080bf"_bytes, -1.0f},
        {"ffff7f3f"_bytes, std::nextafter(1.0f, 0.0f)},
        {"ffff7fbf"_bytes, std::nextafter(-1.0f, 0.0f)},
        {"0000c07f"_bytes, FP32::nan(FP32::canon)},
        {"0100c07f"_bytes, FP32::nan(FP32::canon + 1)},
        {"0100807f"_bytes, FP32::nan(1)},
    };

    uint32_t address = 0;
    for (const auto& [memory_fill, expected] : test_cases)
    {
        std::copy(memory_fill.begin(), memory_fill.end(), instance->memory->data() + address);

        EXPECT_THAT(execute(*instance, 0, {address}), Result(expected));
        address += static_cast<uint32_t>(memory_fill.size());
    }

    EXPECT_THAT(execute(*instance, 0, {65534}), Traps());
    EXPECT_THAT(execute(*instance, 0, {65537}), Traps());
}

TEST(execute_floating_point, f32_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f32)
      get_local 0
      f32.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017d030201000504010101010a0d010b0020002a02ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute_floating_point, f64_load)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f64)
      (f64.load (local.get 0))
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017c030201000504010101010a0901070020002b03000b");

    auto instance = instantiate(parse(wasm));

    const std::tuple<bytes, double> test_cases[]{
        {"0000000000000000"_bytes, 0.0},
        {"0000000000000080"_bytes, -0.0},
        {"5839b4c876bef33f"_bytes, 1.234},
        {"5839b4c876bef3bf"_bytes, -1.234},
        {"000000000000f07f"_bytes, FP64::Limits::infinity()},
        {"000000000000f0ff"_bytes, -FP64::Limits::infinity()},
        {"ffffffffffffef7f"_bytes, FP64::Limits::max()},
        {"ffffffffffffefff"_bytes, -FP64::Limits::max()},
        {"0000000000001000"_bytes, FP64::Limits::min()},
        {"0000000000001080"_bytes, -FP64::Limits::min()},
        {"0100000000000000"_bytes, FP64::Limits::denorm_min()},
        {"0100000000000080"_bytes, -FP64::Limits::denorm_min()},
        {"000000000000f03f"_bytes, 1.0},
        {"000000000000f0bf"_bytes, -1.0},
        {"ffffffffffffef3f"_bytes, std::nextafter(1.0, 0.0)},
        {"ffffffffffffefbf"_bytes, std::nextafter(-1.0, 0.0)},
        {"000000000000f87f"_bytes, FP64::nan(FP64::canon)},
        {"010000000000f87f"_bytes, FP64::nan(FP64::canon + 1)},
        {"010000000000f07f"_bytes, FP64::nan(1)},
    };

    uint32_t address = 0;
    for (const auto& [memory_fill, expected] : test_cases)
    {
        std::copy(memory_fill.begin(), memory_fill.end(), instance->memory->data() + address);

        EXPECT_THAT(execute(*instance, 0, {address}), Result(expected));
        address += static_cast<uint32_t>(memory_fill.size());
    }

    EXPECT_THAT(execute(*instance, 0, {65534}), Traps());
    EXPECT_THAT(execute(*instance, 0, {65537}), Traps());
}

TEST(execute_floating_point, f64_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f64)
      get_local 0
      f64.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017c030201000504010101010a0d010b0020002b03ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute_floating_point, f32_store)
{
    /* wat2wasm
    (memory 1 1)
    (data (i32.const 0)  "\cc\cc\cc\cc\cc\cc")
    (func (param f32 i32)
      get_local 1
      get_local 0
      f32.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027d7f00030201000504010101010a0b010900200120003802000b0b0c01004100"
        "0b06cccccccccccc");
    const auto module = parse(wasm);

    const std::tuple<float, bytes> test_cases[]{
        {0.0f, "cc00000000cc"_bytes},
        {-0.0f, "cc00000080cc"_bytes},
        {1.234f, "ccb6f39d3fcc"_bytes},
        {-1.234f, "ccb6f39dbfcc"_bytes},
        {FP32::Limits::infinity(), "cc0000807fcc"_bytes},
        {-FP32::Limits::infinity(), "cc000080ffcc"_bytes},
        {FP32::Limits::max(), "ccffff7f7fcc"_bytes},
        {-FP32::Limits::max(), "ccffff7fffcc"_bytes},
        {FP32::Limits::min(), "cc00008000cc"_bytes},
        {-FP32::Limits::min(), "cc00008080cc"_bytes},
        {FP32::Limits::denorm_min(), "cc01000000cc"_bytes},
        {-FP32::Limits::denorm_min(), "cc01000080cc"_bytes},
        {1.0f, "cc0000803fcc"_bytes},
        {-1.0f, "cc000080bfcc"_bytes},
        {std::nextafter(1.0f, 0.0f), "ccffff7f3fcc"_bytes},
        {std::nextafter(-1.0f, 0.0f), "ccffff7fbfcc"_bytes},
        {FP32::nan(FP32::canon), "cc0000c07fcc"_bytes},
        {FP32::nan(FP32::canon + 1), "cc0100c07fcc"_bytes},
        {FP32::nan(1), "cc0100807fcc"_bytes},
    };

    for (const auto& [arg, expected] : test_cases)
    {
        auto instance = instantiate(module);

        EXPECT_THAT(execute(*instance, 0, {arg, 1}), Result());
        EXPECT_EQ(instance->memory->substr(0, 6), expected);

        EXPECT_THAT(execute(*instance, 0, {arg, 65534}), Traps());
        EXPECT_THAT(execute(*instance, 0, {arg, 65537}), Traps());
    }
}

TEST(execute_floating_point, f32_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      get_local 0
      f32.const 1.234
      f32.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a12011000200043b6f39d3f3802ffffffff070"
        "b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute_floating_point, f64_store)
{
    /* wat2wasm
    (memory 1 1)
    (data (i32.const 0)  "\cc\cc\cc\cc\cc\cc\cc\cc\cc\cc\cc\cc")
    (func (param f64 i32)
      get_local 1
      get_local 0
      f64.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027c7f00030201000504010101010a0b010900200120003903000b0b1201004100"
        "0b0ccccccccccccccccccccccccc");
    const auto module = parse(wasm);

    const std::tuple<double, bytes> test_cases[]{
        {0.0, "cc0000000000000000cc"_bytes},
        {-0.0, "cc0000000000000080cc"_bytes},
        {1.234, "cc5839b4c876bef33fcc"_bytes},
        {-1.234, "cc5839b4c876bef3bfcc"_bytes},
        {FP64::Limits::infinity(), "cc000000000000f07fcc"_bytes},
        {-FP64::Limits::infinity(), "cc000000000000f0ffcc"_bytes},
        {FP64::Limits::max(), "ccffffffffffffef7fcc"_bytes},
        {-FP64::Limits::max(), "ccffffffffffffefffcc"_bytes},
        {FP64::Limits::min(), "cc0000000000001000cc"_bytes},
        {-FP64::Limits::min(), "cc0000000000001080cc"_bytes},
        {FP64::Limits::denorm_min(), "cc0100000000000000cc"_bytes},
        {-FP64::Limits::denorm_min(), "cc0100000000000080cc"_bytes},
        {1.0, "cc000000000000f03fcc"_bytes},
        {-1.0, "cc000000000000f0bfcc"_bytes},
        {std::nextafter(1.0, 0.0), "ccffffffffffffef3fcc"_bytes},
        {std::nextafter(-1.0, 0.0), "ccffffffffffffefbfcc"_bytes},
        {FP64::nan(FP64::canon), "cc000000000000f87fcc"_bytes},
        {FP64::nan(FP64::canon + 1), "cc010000000000f87fcc"_bytes},
        {FP64::nan(1), "cc010000000000f07fcc"_bytes},
    };

    for (const auto& [arg, expected] : test_cases)
    {
        auto instance = instantiate(module);

        EXPECT_THAT(execute(*instance, 0, {arg, 1}), Result());
        EXPECT_EQ(instance->memory->substr(0, 10), expected);

        EXPECT_THAT(execute(*instance, 0, {arg, 65534}), Traps());
        EXPECT_THAT(execute(*instance, 0, {arg, 65537}), Traps());
    }
}

TEST(execute_floating_point, f64_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      get_local 0
      f64.const 1.234
      f64.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a160114002000445839b4c876bef33f3903ffff"
        "ffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}
