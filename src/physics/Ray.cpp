#include "physics/Ray.h"

#include "VMath.h"
#include "MMath.h"
#include "QMath.h"
#include "core/Components.h"
#include "core/Camera.h"

PHYSICS::Ray PHYSICS::ToLocalRay(const Ray& worldRay, const Transform& transform)
{
    /* We have to reverse the process of a transform */
    const MATH::Quaternion orientation = MATH::QMath::conjugate(transform.orientation);
    MATH::Vec3 position = (worldRay.origin - transform.position);
    position = MATH::QMath::rotate(position, orientation);
    position /= transform.scale;
    return {
        .origin = position,
        .direction = MATH::QMath::rotate(worldRay.direction, orientation) / transform.scale
    };
}

PHYSICS::Ray PHYSICS::MakePickRay(const MATH::Vec2& mousePixel, const MATH::Vec2& windowSize, const Camera& camera)
{
    const MATH::Vec4 mouseCoords(mousePixel.x, mousePixel.y, 0.0f, 1.0f);
    /* Use the inverse to get PixelSpace -> NDC */
    /* This functions holds the Y flip */
    const MATH::Matrix4 ndcToPixelSpace = MMath::NDCtoViewport(static_cast<int>(windowSize.x), static_cast<int>(windowSize.y));
    MATH::Vec4 mouseCoordsNdc = MMath::inverse(ndcToPixelSpace) * mouseCoords;
    /* Vulkan's depth runs from 0 at the near plane to 1 at the far plane */
    mouseCoordsNdc.z = 0.0f;

    /* NDC to camera space */
    const MATH::Matrix4 cameraToNdcSpace = MATH::MMath::PerspectiveVulkan(
        camera.fovYDegrees, windowSize.x / windowSize.y, camera.nearPlane, camera.farPlane);
    const MATH::Vec4 mouseCoordsCamera = MATH::VMath::perspectiveDivide(
        MATH::MMath::inverse(cameraToNdcSpace) * mouseCoordsNdc);

    /* Camera space to world space. The inverse view's last column is the camera's position */
    const MATH::Matrix4 cameraToWorldSpace = MATH::MMath::inverse(camera.view);
    const MATH::Vec3 mouseCoordsWorld(cameraToWorldSpace * mouseCoordsCamera);
    const MATH::Vec3 cameraPosition(cameraToWorldSpace[12], cameraToWorldSpace[13], cameraToWorldSpace[14]);

    return {
        .origin = cameraPosition,
        .direction = MATH::VMath::normalize(mouseCoordsWorld - cameraPosition)
    };
}
