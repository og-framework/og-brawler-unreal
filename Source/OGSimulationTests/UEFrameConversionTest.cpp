// SPDX-License-Identifier: MPL-2.0
#if WITH_LOW_LEVEL_TESTS

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "CoreMinimal.h"
#include "catch_amalgamated.hpp"
#include "OGSimulation/SimulationFrame.h"
#define OGSIMULATIONUNREAL_API
#include "OGSimulationUnreal/UEFrameConversion.h"
#include "OGSimulationUnreal/UEFrameConversion.cpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"

namespace ueFrameConversionTest
{
    using UeCentimetres = BasicUeFrame<BasicSimFrame<AxisFrame::UeZUpLeftHanded, 100.f>>;
    using YMetres = BasicUeFrame<BasicSimFrame<AxisFrame::YUpRightHanded, 1.f>>;

    static_assert(!UeCentimetres::kReflects, "UEFrameConversion: UE's own frame does not reflect");
    static_assert(YMetres::kReflects, "UEFrameConversion: og's right-handed Y-up frame reflects UE's left-handed Z-up frame");
    static_assert(BasicUeFrame<BasicSimFrame<AxisFrame::YUpRightHanded, 100.f>>::kReflects
            && !BasicUeFrame<BasicSimFrame<AxisFrame::UeZUpLeftHanded, 1.f>>::kReflects,
        "UEFrameConversion: kReflects follows the axis frame only");

    bool sameBits(float a, float b)
    {
        return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
    }

    bool sameBits(double a, double b)
    {
        return std::bit_cast<uint64_t>(a) == std::bit_cast<uint64_t>(b);
    }

    bool sameBits(const glm::vec3& a, const glm::vec3& b)
    {
        return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z);
    }

    bool sameBits(const glm::dvec3& a, const glm::dvec3& b)
    {
        return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z);
    }

    bool sameBits(const glm::quat& a, const glm::quat& b)
    {
        return sameBits(a.w, b.w) && sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z);
    }

    bool sameBits(const FVector& a, const FVector& b)
    {
        return sameBits(a.X, b.X) && sameBits(a.Y, b.Y) && sameBits(a.Z, b.Z);
    }

    bool sameBits(const FQuat& a, const FQuat& b)
    {
        return sameBits(a.X, b.X) && sameBits(a.Y, b.Y) && sameBits(a.Z, b.Z) && sameBits(a.W, b.W);
    }

    bool sameBits(const glm::mat4& a, const glm::mat4& b)
    {
        for (glm::length_t column = 0; column < 4; ++column)
        {
            for (glm::length_t row = 0; row < 4; ++row)
            {
                if (!sameBits(a[column][row], b[column][row]))
                    return false;
            }
        }
        return true;
    }

    std::vector<float> edgeFloats()
    {
        return { 0.f, -0.f, 1.f, -1.f, 0.5f, 42.f, -96.f, 980.f, -980.f, 1.0e-4f, 3.19e-42f, -3.19e-42f, 1234.5678f,
            -98765.4321f, 0.01f, 0.42f, std::numeric_limits<float>::min(), std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
    }

    std::vector<glm::vec3> seededVectors(uint32_t seed, size_t count, float range)
    {
        std::mt19937 generator(seed);
        std::uniform_real_distribution<float> distribution(-range, range);
        std::vector<glm::vec3> vectors;
        for (size_t i = 0; i < count; ++i)
            vectors.emplace_back(distribution(generator), distribution(generator), distribution(generator));
        return vectors;
    }

    std::vector<glm::vec3> edgeAndSeededVectors(uint32_t seed, size_t count, float range)
    {
        std::vector<glm::vec3> vectors;
        const std::vector<float> edges = edgeFloats();
        for (size_t i = 0; i + 2 < edges.size(); ++i)
            vectors.emplace_back(edges[i], edges[i + 1], edges[i + 2]);
        for (const glm::vec3& v : seededVectors(seed, count, range))
            vectors.push_back(v);
        return vectors;
    }

    FQuat roundedToFloats(const FQuat& q)
    {
        return FQuat(static_cast<float>(q.X), static_cast<float>(q.Y), static_cast<float>(q.Z), static_cast<float>(q.W));
    }

    std::vector<FQuat> seededRotations(uint32_t seed, size_t count)
    {
        std::mt19937 generator(seed);
        std::uniform_real_distribution<double> distribution(-1.0, 1.0);
        std::vector<FQuat> rotations = { FQuat::Identity, roundedToFloats(FQuat(FVector(0.0, 0.0, 1.0), 0.5)),
            roundedToFloats(FQuat(FVector(0.0, 1.0, 0.0), -1.25)), roundedToFloats(FQuat(FVector(1.0, 0.0, 0.0), 2.0)) };
        while (rotations.size() < count)
        {
            FQuat q(distribution(generator), distribution(generator), distribution(generator), distribution(generator));
            if (q.SizeSquared() < 1.0e-3)
                continue;
            q.Normalize();
            rotations.push_back(roundedToFloats(q));
        }
        return rotations;
    }

    FVector fromFloats(const glm::vec3& v)
    {
        return FVector(v.x, v.y, v.z);
    }

    glm::vec3 reference(const FVector& v)
    {
        glm::vec3 out;
        out.x = v.X;
        out.y = v.Y;
        out.z = v.Z;
        return out;
    }

    glm::mat4 referenceMatrix(const FTransform& transform)
    {
        const FVector translation = transform.GetTranslation();
        const FQuat rotation = transform.GetRotation();
        const FVector scale = transform.GetScale3D();
        glm::mat4 matrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation.X, translation.Y, translation.Z));
        matrix *= glm::mat4_cast(glm::quat(rotation.W, rotation.X, rotation.Y, rotation.Z));
        return glm::scale(matrix, glm::vec3(scale.X, scale.Y, scale.Z));
    }

    template <class F>
    void checkRoundTrips()
    {
        for (const glm::vec3& og : edgeAndSeededVectors(7u, 2000u, 5000.f))
        {
            const FVector ue = F::vectorToUe(og);
            CHECK(sameBits(F::vectorToSim(ue), og));
            CHECK(sameBits(F::vectorToUe(F::vectorToSim(ue)), ue));
            CHECK(sameBits(F::directionToSim(F::directionToUe(og)), og));
            CHECK(sameBits(F::axialToSim(F::axialToUe(og)), og));
            CHECK(sameBits(F::torqueToSim(F::torqueToUe(og)), og));
            CHECK(sameBits(F::inertiaToSim(F::inertiaToUe(og)), og));
            CHECK(sameBits(F::halfExtentsToSim(F::halfExtentsToUe(og)), og));
            CHECK(sameBits(F::scaleToSim(F::scaleToUe(og)), og));
            CHECK(sameBits(F::lengthToSim(F::lengthToUe(og.x)), og.x));
            const glm::dvec3 simD = F::vectorToSimD(ue);
            CHECK(sameBits(glm::vec3(static_cast<float>(simD.x), static_cast<float>(simD.y), static_cast<float>(simD.z)), og));
        }
        for (const FQuat& ue : seededRotations(11u, 500u))
        {
            const glm::quat og = F::rotationToSim(ue);
            CHECK(sameBits(F::rotationToSim(F::rotationToUe(og)), og));
            CHECK(sameBits(F::rotationToUe(og), ue));
        }
    }

    template <class F>
    void checkQuaternionRule()
    {
        const std::vector<glm::vec3> directions = seededVectors(13u, 200u, 1.f);
        for (const FQuat& ue : seededRotations(17u, 200u))
        {
            const glm::quat og = F::rotationToSim(ue);
            for (const glm::vec3& direction : directions)
            {
                const FVector v = fromFloats(direction);
                const glm::vec3 rotatedThenMapped = F::directionToSim(ue.RotateVector(v));
                const glm::vec3 mappedThenRotated = og * F::directionToSim(v);
                CHECK(glm::length(rotatedThenMapped - mappedThenRotated) <= 1.0e-6f);
            }
        }
    }

    template <class F>
    void checkTransformRule()
    {
        const std::vector<glm::vec3> points = seededVectors(19u, 100u, 3000.f);
        const std::vector<FQuat> rotations = seededRotations(23u, 50u);
        for (size_t r = 0; r < rotations.size(); ++r)
        {
            const FVector translation = fromFloats(points[r]);
            const FTransform ue(rotations[r], translation);
            const glm::mat4 og = F::transformToSim(ue);
            for (const glm::vec3& point : points)
            {
                const FVector local = fromFloats(point);
                const glm::vec3 expected = F::vectorToSim(ue.TransformPosition(local));
                const glm::vec3 actual = glm::vec3(og * glm::vec4(F::vectorToSim(local), 1.f));
                CHECK(glm::length(expected - actual) <= 1.0e-6f * (1.f + glm::length(expected)) * 8.f);
            }
            const FTransform back = F::transformToUe(og);
            CHECK(back.GetTranslation().Equals(ue.GetTranslation(), 1.0e-3 * (1.0 + ue.GetTranslation().Size())));
            CHECK(back.GetRotation().Equals(ue.GetRotation(), 1.0e-5));
            CHECK(back.GetScale3D().Equals(FVector::OneVector, 1.0e-5));
        }
    }

    template <class F>
    void checkWindingRule()
    {
        const double s = 1.0 / F::kUeCentimetresPerUnit;
        const std::vector<glm::vec3> corners = seededVectors(29u, 300u, 400.f);
        for (bool mirrored : { false, true })
        {
            for (size_t i = 0; i + 2 < corners.size(); i += 3)
            {
                const FVector ue[3] = { fromFloats(corners[i]), fromFloats(corners[i + 1]), fromFloats(corners[i + 2]) };
                const FVector ueNormal = FVector::CrossProduct(ue[1] - ue[0], ue[2] - ue[0]);
                if (ueNormal.Size() < 1.0)
                    continue;
                const bool swap = F::reversesWinding(mirrored);
                const glm::dvec3 og[3] = { F::vectorToSimD(ue[0]), F::vectorToSimD(swap ? ue[2] : ue[1]),
                    F::vectorToSimD(swap ? ue[1] : ue[2]) };
                const glm::dvec3 ogNormal = glm::cross(og[1] - og[0], og[2] - og[0]);
                const glm::vec3 direction = F::directionToSim(ueNormal);
                const glm::dvec3 expected = glm::dvec3(direction) * (mirrored ? -1.0 : 1.0) * s * s;
                CHECK(glm::length(ogNormal - expected) <= 1.0e-6 * glm::length(expected));
            }
        }
    }
TEST_CASE("UEFrameConversion.UeCentimetresIsTodaysComponentCopy", "[SimulationFrame][UEFrameConversion]")
{
    for (const glm::vec3& og : edgeAndSeededVectors(31u, 2000u, 5000.f))
    {
        const FVector ue = fromFloats(og);
        CHECK(sameBits(UeCentimetres::vectorToSim(ue), reference(ue)));
        CHECK(sameBits(UeCentimetres::directionToSim(ue), reference(ue)));
        CHECK(sameBits(UeCentimetres::axialToSim(ue), reference(ue)));
        CHECK(sameBits(UeCentimetres::torqueToSim(ue), reference(ue)));
        CHECK(sameBits(UeCentimetres::inertiaToSim(ue), reference(ue)));
        CHECK(sameBits(UeCentimetres::halfExtentsToSim(ue), reference(ue)));
        CHECK(sameBits(UeCentimetres::vectorToSimD(ue), glm::dvec3(ue.X, ue.Y, ue.Z)));
        CHECK(sameBits(UeCentimetres::vectorToUe(og), ue));
        CHECK(sameBits(UeCentimetres::directionToUe(og), ue));
        CHECK(sameBits(UeCentimetres::axialToUe(og), ue));
        CHECK(sameBits(UeCentimetres::torqueToUe(og), ue));
        CHECK(sameBits(UeCentimetres::inertiaToUe(og), ue));
        CHECK(sameBits(UeCentimetres::halfExtentsToUe(og), ue));
        CHECK(sameBits(UeCentimetres::lengthToSim(ue.X), og.x));
        CHECK(sameBits(UeCentimetres::lengthToUe(og.x), ue.X));
        CHECK(sameBits(ueNative::toGlm(ue), reference(ue)));
        CHECK(sameBits(ueNative::toFVector(og), ue));
    }
    for (const FQuat& ue : seededRotations(37u, 500u))
    {
        const glm::quat copy(static_cast<float>(ue.W), static_cast<float>(ue.X), static_cast<float>(ue.Y), static_cast<float>(ue.Z));
        CHECK(sameBits(UeCentimetres::rotationToSim(ue), copy));
        CHECK(sameBits(UeCentimetres::rotationToUe(copy), FQuat(copy.x, copy.y, copy.z, copy.w)));
        CHECK(sameBits(ueNative::toGlmQuat(ue), copy));
        const FTransform transform(ue, FVector(12.5, -340.25, 96.0));
        CHECK(UeCentimetres::transformToSim(transform) == referenceMatrix(transform));
        CHECK(sameBits(UeCentimetres::transformToSim(transform), ueNative::toGLMMat4(transform)));
        const FTransform back = UeCentimetres::transformToUe(UeCentimetres::transformToSim(transform));
        CHECK(sameBits(back.GetTranslation(), ueNative::toFTransform(UeCentimetres::transformToSim(transform)).GetTranslation()));
        CHECK(sameBits(back.GetRotation(), ueNative::toFTransform(UeCentimetres::transformToSim(transform)).GetRotation()));
    }
    CHECK(UeCentimetres::reversesWinding(true));
    CHECK(!UeCentimetres::reversesWinding(false));
}

TEST_CASE("UEFrameConversion.YMetresMapsTheDesignTable", "[SimulationFrame][UEFrameConversion]")
{
    const FVector ue(100.0, 200.0, 300.0);
    CHECK(sameBits(YMetres::vectorToSim(ue), glm::vec3(2.f, 3.f, -1.f)));
    CHECK(sameBits(YMetres::directionToSim(ue), glm::vec3(200.f, 300.f, -100.f)));
    CHECK(sameBits(YMetres::axialToSim(ue), glm::vec3(-200.f, -300.f, 100.f)));
    CHECK(sameBits(YMetres::torqueToSim(FVector(1.0e4, 2.0e4, 3.0e4)), glm::vec3(-2.f, -3.f, 1.f)));
    CHECK(sameBits(YMetres::inertiaToSim(FVector(1.0e4, 2.0e4, 3.0e4)), glm::vec3(2.f, 3.f, 1.f)));
    CHECK(sameBits(YMetres::halfExtentsToSim(ue), glm::vec3(2.f, 3.f, 1.f)));
    CHECK(sameBits(YMetres::vectorToUe(glm::vec3(2.f, 3.f, -1.f)), ue));
    CHECK(sameBits(YMetres::lengthToSim(42.0), 0.42f));
    CHECK(sameBits(YMetres::lengthToUe(0.5f), 50.0));
    const glm::quat og = YMetres::rotationToSim(FQuat(0.1, 0.2, 0.3, 0.9));
    CHECK(sameBits(og, glm::quat(0.9f, -0.2f, -0.3f, 0.1f)));
    CHECK(sameBits(YMetres::rotationToUe(og), FQuat(double(0.1f), double(0.2f), double(0.3f), double(0.9f))));
    CHECK(YMetres::reversesWinding(false));
    CHECK(!YMetres::reversesWinding(true));
}

TEST_CASE("UEFrameConversion.RoundTripsAreBitExactAtBothInstantiations", "[SimulationFrame][UEFrameConversion]")
{
    checkRoundTrips<UeCentimetres>();
    checkRoundTrips<YMetres>();
}

TEST_CASE("UEFrameConversion.QuaternionRuleHoldsAtBothInstantiations", "[SimulationFrame][UEFrameConversion]")
{
    checkQuaternionRule<UeCentimetres>();
    checkQuaternionRule<YMetres>();
}

TEST_CASE("UEFrameConversion.TransformRuleHoldsAtBothInstantiations", "[SimulationFrame][UEFrameConversion]")
{
    checkTransformRule<UeCentimetres>();
    checkTransformRule<YMetres>();
}

TEST_CASE("UEFrameConversion.WindingRuleKeepsTheOutwardNormalAtBothInstantiations", "[SimulationFrame][UEFrameConversion]")
{
    checkWindingRule<UeCentimetres>();
    checkWindingRule<YMetres>();
}
} // namespace ueFrameConversionTest

#endif
