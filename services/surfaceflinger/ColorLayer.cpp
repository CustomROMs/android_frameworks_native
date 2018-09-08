/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "ColorLayer"

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/Log.h>

#include <ui/GraphicBuffer.h>

#include "ColorLayer.h"
#include "DisplayDevice.h"
#include "RenderEngine/RenderEngine.h"
#include "SurfaceFlinger.h"

namespace android {
// ---------------------------------------------------------------------------

ColorLayer::ColorLayer(SurfaceFlinger* flinger, const sp<Client>& client, const String8& name,
                       uint32_t w, uint32_t h, uint32_t flags)
      : Layer(flinger, client, name, w, h, flags) {
    // drawing state & current state are identical
    mDrawingState = mCurrentState;
}

void ColorLayer::onDraw(const RenderArea& renderArea, const Region& /* clip */,
                        bool useIdentityTransform) const {
    half4 color = getColor();
    if (color.a > 0) {
        Mesh mesh(Mesh::TRIANGLE_FAN, 4, 2);
        computeGeometry(renderArea, mesh, useIdentityTransform);
        auto& engine(mFlinger->getRenderEngine());
        engine.setupLayerBlending(getPremultipledAlpha(), false /* opaque */,
                                  true /* disableTexture */, color);
        engine.drawMesh(mesh);
        engine.disableBlending();
    }
}

bool ColorLayer::isVisible() const {
    const Layer::State& s(getDrawingState());
    return !isHiddenByPolicy() && s.color.a;
}

// ---------------------------------------------------------------------------

}; // namespace android
