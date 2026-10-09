-- Simulator only. Do not open an OpenXR runtime (WiVRn blocks without a headset).
function lovr.conf(t)
	t.identity = "engine"
	t.window.title = "engine"
	t.window.width = 1280
	t.window.height = 480
	t.window.vsync = false
	t.graphics.vsync = false
	t.headset.drivers = { "simulator" }
end
