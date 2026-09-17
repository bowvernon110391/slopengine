#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

struct RenderItem;

// A reference to a RenderItem, stored as an index (never a pointer) so it can
// never dangle if the item vector is rebuilt.
struct DrawCommand
{
    std::uint32_t itemIndex = 0;
    std::uint32_t sortKey   = 0;

    DrawCommand() = default;
    DrawCommand(std::uint32_t index, std::uint32_t key) : itemIndex(index), sortKey(key) {}
};

// An ordered list of draw commands produced by RenderQueue.
class DrawList
{
public:
    void clear() { m_commands.clear(); }
    void reserve(std::size_t n) { m_commands.reserve(n); }

    void push(std::uint32_t itemIndex, std::uint32_t sortKey = 0)
    {
        m_commands.push_back(DrawCommand{ itemIndex, sortKey });
    }

    bool        empty() const { return m_commands.empty(); }
    std::size_t size()  const { return m_commands.size(); }

    const std::vector<DrawCommand>& commands() const { return m_commands; }

    // 'items' is RenderQueue::frameItems: the array the indices refer to.
    void sortByMaterial(const std::vector<RenderItem>& items);
    void sortFrontToBack(const std::vector<RenderItem>& items, const glm::vec3& cameraPos);
    void sortBackToFront(const std::vector<RenderItem>& items, const glm::vec3& cameraPos);

private:
    std::vector<DrawCommand> m_commands;
};
