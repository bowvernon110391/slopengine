#include "renderer/DrawList.h"

#include <algorithm>

#include "renderer/RenderItem.h"

namespace {

float depthSqr(const RenderItem& item, const glm::vec3& cameraPos)
{
    const glm::vec3 d = item.worldBounds.center() - cameraPos;
    return glm::dot(d, d);
}

} // namespace

void DrawList::sortByMaterial(const std::vector<RenderItem>& items, const glm::vec3& cameraPos)
{
    std::stable_sort(m_commands.begin(), m_commands.end(),
                     [&items, &cameraPos](const DrawCommand& a, const DrawCommand& b) {
                         const RenderItem& ia = items[a.itemIndex];
                         const RenderItem& ib = items[b.itemIndex];
                         if (ia.material != ib.material) return ia.material < ib.material;
                         // Same material: nearest first so early-z still helps.
                         return depthSqr(ia, cameraPos) < depthSqr(ib, cameraPos);
                     });
}

void DrawList::sortFrontToBack(const std::vector<RenderItem>& items, const glm::vec3& cameraPos)
{
    std::stable_sort(m_commands.begin(), m_commands.end(),
                     [&items, &cameraPos](const DrawCommand& a, const DrawCommand& b) {
                         return depthSqr(items[a.itemIndex], cameraPos)
                              < depthSqr(items[b.itemIndex], cameraPos);
                     });
}

void DrawList::sortBackToFront(const std::vector<RenderItem>& items, const glm::vec3& cameraPos)
{
    std::stable_sort(m_commands.begin(), m_commands.end(),
                     [&items, &cameraPos](const DrawCommand& a, const DrawCommand& b) {
                         return depthSqr(items[a.itemIndex], cameraPos)
                              > depthSqr(items[b.itemIndex], cameraPos);
                     });
}
