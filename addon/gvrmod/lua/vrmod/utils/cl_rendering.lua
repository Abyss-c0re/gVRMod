g_VR = g_VR or {}
vrmod = vrmod or {}
vrmod.utils = vrmod.utils or {}
function vrmod.utils.CalculateProjectionParams(projMatrix, worldScale)
    local xscale = projMatrix[1][1]
    local xoffset = projMatrix[1][3]
    local yscale = projMatrix[2][2]
    local yoffset = projMatrix[2][3]
    -- ** Normalize vertical sign: **
    if not system.IsWindows() then
        -- On Linux/OpenGL: invert the sign so + means “down” just like on Windows
        yoffset = -yoffset
    end

    -- now the rest is identical on both platforms:
    local tan_px = math.abs((1 - xoffset) / xscale)
    local tan_nx = math.abs((-1 - xoffset) / xscale)
    local tan_py = math.abs((1 - yoffset) / yscale)
    local tan_ny = math.abs((-1 - yoffset) / yscale)
    local w = (tan_px + tan_nx) / worldScale
    local h = (tan_py + tan_ny) / worldScale
    return {
        HorizontalFOV = math.deg(2 * math.atan(w / 2)),
        AspectRatio = w / h,
        HorizontalOffset = xoffset,
        VerticalOffset = yoffset,
        Width = w,
        Height = h,
    }
end

-- Legacy desktop crop helper (was computing sub-rect + shift inside the side-by-side RT).
-- With per-eye RTs the desktop preview just shows one eye's RT; a simple aspect fit is done inline.
function vrmod.utils.ComputeDesktopCrop(desktopView, w, h)
    -- Return values that select "full" for the chosen eye RT when used with DrawTexturedRectUV on a per-eye material.
    return 0, 0
end

-- Legacy: ComputeSubmitBounds was used to compute UV sub-rects when both eyes were packed
-- side-by-side into a single *2-wide RT, with extra offsets to compensate for asymmetric
-- frustums. With proper per-eye RTs + per-eye submission we render each eye to its own
-- full-size RT using its exact projection; submit uses (near) full UV per eye.
-- This function is kept only for backward compat / debug tools; it now returns a simple
-- full-rect mapping (no packing).
function vrmod.utils.ComputeSubmitBounds(leftCalc, rightCalc, hOffset, vOffset, scaleFactor, renderOffset)
    local TEXTURE_INSET = 0.003
    -- Full rects for each eye (left and right halves of a fictional packed buffer are ignored).
    local uMinLeft, vMinLeft, uMaxLeft, vMaxLeft = TEXTURE_INSET, TEXTURE_INSET, 1 - TEXTURE_INSET, 1 - TEXTURE_INSET
    local uMinRight, vMinRight, uMaxRight, vMaxRight = TEXTURE_INSET, TEXTURE_INSET, 1 - TEXTURE_INSET, 1 - TEXTURE_INSET
    return uMinLeft, vMinLeft, uMaxLeft, vMaxLeft, uMinRight, vMinRight, uMaxRight, vMaxRight
end

function vrmod.utils.AdjustFOV(proj, fovScaleX, fovScaleY)
    local clone = {}
    for i = 1, 4 do
        clone[i] = {proj[i][1], proj[i][2], proj[i][3], proj[i][4]}
    end

    -- scale the FOV (diagonal terms)
    clone[1][1] = clone[1][1] * fovScaleX
    clone[2][2] = clone[2][2] * fovScaleY
    -- scale the center offset (asymmetry) terms
    clone[1][3] = clone[1][3] * fovScaleX
    clone[2][3] = clone[2][3] * fovScaleY
    return clone
end

function vrmod.utils.DrawDeathAnimation(rtWidth, rtHeight)
    if not g_VR.deathTime then g_VR.deathTime = CurTime() end
    local fadeAlpha = 0
    local fadeDuration = 3.5
    local maxAlpha = 200
    local progress = math.min((CurTime() - g_VR.deathTime) / fadeDuration, 1)
    fadeAlpha = math.min(progress * maxAlpha, maxAlpha)
    cam.Start2D()
    surface.SetDrawColor(120, 0, 0, fadeAlpha)
    surface.DrawRect(0, 0, rtWidth, rtHeight)
    cam.End2D()
end