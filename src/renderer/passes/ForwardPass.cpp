#include "renderer/passes/ForwardPass.h"

#include <vector>

#include "Mesh.h"
#include "Shader.h"
#include "renderer/RenderQueue.h"
#include "renderer/RenderTargetPool.h"
#include "renderer/ShaderCache.h"
#include "scene/Scene.h"

bool ForwardPass::setup(RenderContext& ctx)
{
    m_shader = ctx.shaders ? ctx.shaders->get("forward") : nullptr;
    return m_shader != nullptr;
}

void ForwardPass::execute(RenderContext& ctx)
{
    if (!m_shader || !ctx.pool || !ctx.queue || !ctx.view) return;
    if (!ctx.pool->valid(ctx.fboMainMS)) return;

    gl::BindFramebuffer(gl::FRAMEBUFFER, ctx.pool->fbo(ctx.fboMainMS));
    gl::Viewport(ctx.viewport.x, ctx.viewport.y, ctx.viewport.width, ctx.viewport.height);

    gl::Enable(gl::DEPTH_TEST);
    gl::DepthFunc(gl::LEQUAL);
    gl::DepthMask(1);
    gl::Enable(gl::CULL_FACE);
    gl::CullFace(gl::BACK);
    gl::FrontFace(gl::CCW);
    gl::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
    gl::Clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

    m_shader->use();
    m_shader->setMat4("uProjection", ctx.view->camera.projectionMatrix());
    m_shader->setMat4("uView", ctx.view->camera.viewMatrix());
    m_shader->setVec3("uCameraPos", ctx.view->camera.position());

    // --- Ambient (per scene, shadows do not attenuate it) -----------------
    const glm::vec3 ambient = ctx.scene ? glm::max(ctx.scene->ambient, glm::vec3(0.0f))
                                        : glm::vec3(0.18f);
    m_shader->setVec3("uAmbient", ambient);

    // --- Shadow range/fade -------------------------------------------------
    const float shadowDistance = ctx.shadowFit ? ctx.shadowFit->shadowDistance : 40.0f;
    const float shadowFade     = ctx.shadowFit ? ctx.shadowFit->fadeFraction   : 0.0f;
    m_shader->setFloat("uShadowDistance", shadowDistance);
    m_shader->setFloat("uShadowFade", shadowFade);

    // --- Directional light (first one in the view) -----------------------
    const Light* dirLight = nullptr;
    for (const Light& l : ctx.view->lights) {
        if (l.type == LightType::Directional) { dirLight = &l; break; }
    }
    if (dirLight) {
        m_shader->setInt("uDirLightEnabled", 1);
        m_shader->setVec3("uDirLightDir", dirLight->direction);
        m_shader->setVec3("uDirLightColor", dirLight->color * dirLight->intensity);
    } else {
        m_shader->setInt("uDirLightEnabled", 0);
    }

    // --- Point light array ------------------------------------------------
    std::vector<glm::vec3> pointPos;
    std::vector<glm::vec3> pointColor;
    std::vector<float>     pointIntensity;
    std::vector<float>     pointRange;
    for (const Light& l : ctx.view->lights) {
        if (l.type != LightType::Point) continue;
        if (static_cast<int>(pointPos.size()) >= kMaxPointLights) break;
        pointPos.push_back(l.position);
        pointColor.push_back(l.color);
        pointIntensity.push_back(l.intensity);
        pointRange.push_back(l.range);
    }
    const int pointCount = static_cast<int>(pointPos.size());
    m_shader->setInt("uPointLightCount", pointCount);
    if (pointCount > 0) {
        m_shader->setVec3Array ("uPointPos",       pointCount, pointPos.data());
        m_shader->setVec3Array ("uPointColor",     pointCount, pointColor.data());
        m_shader->setFloatArray("uPointIntensity", pointCount, pointIntensity.data());
        m_shader->setFloatArray("uPointRange",     pointCount, pointRange.data());
    }

    // --- Shadow map (the first shadow-casting light) ----------------------
    int shadowEnabled = 0;
    if (!ctx.fboShadow.empty() && !ctx.lightSpaceMatrices.empty()) {
        const gl::GLuint depthTex = ctx.pool->depthTexture(ctx.fboShadow[0]);
        if (depthTex != 0) {
            gl::ActiveTexture(gl::TEXTURE0);
            gl::BindTexture(gl::TEXTURE_2D, depthTex);
            m_shader->setInt("uShadowMap", 0);
            m_shader->setMat4("uLightSpaceMatrix", ctx.lightSpaceMatrices[0]);
            shadowEnabled = 1;
        }
    }
    m_shader->setInt("uShadowEnabled", shadowEnabled);

    // --- Draw -------------------------------------------------------------
    const std::vector<RenderItem>& items = ctx.queue->frameItems();

    auto drawList = [&](const DrawList& list) {
        // Opaque draws arrive grouped by material, so the material uniforms only
        // need re-uploading when the batch changes rather than per draw.
        const Material* lastMaterial = nullptr;
        bool haveMaterialState = false;

        for (const DrawCommand& cmd : list.commands()) {
            if (cmd.itemIndex >= items.size()) continue;
            const RenderItem& item = items[cmd.itemIndex];
            if (!item.mesh) continue;

            glm::vec3 albedo(1.0f);
            float specPower = 32.0f, specStrength = 0.35f;
            if (item.material) {
                albedo       = item.material->albedo;
                specPower    = item.material->specPower;
                specStrength = item.material->specStrength;
            }

            m_shader->setMat4("uModel", item.model);

            if (!haveMaterialState || item.material != lastMaterial) {
                m_shader->setVec3 ("uAlbedo",       albedo);
                m_shader->setFloat("uSpecPower",    specPower);
                m_shader->setFloat("uSpecStrength", specStrength);
                lastMaterial = item.material;
                haveMaterialState = true;
            }
            item.mesh->draw();
        }
    };

    drawList(ctx.queue->opaqueList());
    drawList(ctx.queue->transparentList());

    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}
