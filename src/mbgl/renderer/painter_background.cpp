#include <mbgl/renderer/painter.hpp>
#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/style/layers/background_layer.hpp>
#include <mbgl/style/layers/background_layer_impl.hpp>
#include <mbgl/programs/programs.hpp>
#include <mbgl/programs/fill_program.hpp>
#include <mbgl/sprite/sprite_atlas.hpp>
#include <mbgl/util/tile_cover.hpp>

namespace mbgl {

using namespace style;

void Painter::renderBackground(PaintParameters& parameters, const BackgroundLayer& layer) {
    // Note that for bottommost layers without a pattern, the background color is drawn with
    // glClear rather than this method.
    const BackgroundPaintProperties::Evaluated& background = layer.impl->paint.evaluated;

    style::FillPaintProperties::Evaluated properties;
    properties.get<FillPattern>() = background.get<BackgroundPattern>();
    properties.get<FillOpacity>() = { background.get<BackgroundOpacity>() };
    properties.get<FillColor>() = { background.get<BackgroundColor>() };

    const FillProgram::PaintPropertyBinders paintAttibuteData(properties, 0);

    if (!background.get<BackgroundPattern>().to.empty()) {
        optional<SpriteAtlasPosition> imagePosA = spriteAtlas->getPosition(
            background.get<BackgroundPattern>().from, SpritePatternMode::Repeating);
        optional<SpriteAtlasPosition> imagePosB = spriteAtlas->getPosition(
            background.get<BackgroundPattern>().to, SpritePatternMode::Repeating);

        if (!imagePosA || !imagePosB)
            return;

        spriteAtlas->bind(true, context, 0);

        for (const auto& tileID : util::tileCover(state, state.getIntegerZoom())) {
            parameters.programs.fillPattern.draw(
                context,
                gl::Triangles(),
                depthModeForSublayer(0, gl::DepthMode::ReadOnly),
                gl::StencilMode::disabled(),
                colorModeForRenderPass(),
                FillPatternUniforms::values(
                    matrixForTile(tileID),
                    context.viewport.getCurrentValue().size,
                    *imagePosA,
                    *imagePosB,
                    background.get<BackgroundPattern>(),
                    tileID,
                    state
                ),
                tileVertexBuffer,
                tileTriangleIndexBuffer,
                tileTriangleSegments,
                paintAttibuteData,
                properties,
                state.getZoom()
            );
        }
    } else {
        for (const auto& tileID : util::tileCover(state, state.getIntegerZoom())) {
            parameters.programs.fill.draw(
                context,
                gl::Triangles(),
                depthModeForSublayer(0, gl::DepthMode::ReadOnly),
                gl::StencilMode::disabled(),
                colorModeForRenderPass(),
                FillProgram::UniformValues {
                    uniforms::u_matrix::Value{ matrixForTile(tileID) },
                    uniforms::u_world::Value{ context.viewport.getCurrentValue().size },
                },
                tileVertexBuffer,
                tileTriangleIndexBuffer,
                tileTriangleSegments,
                paintAttibuteData,
                properties,
                state.getZoom()
            );
        }
    }
}

} // namespace mbgl
