-- Optional, narrowly scoped Hyprland 0.57+ rule for Aurora Studio.
-- Add this to user-maintained Hyprland Lua configuration if centering is
-- ignored by the Wayland compositor. Do not put it in DMS-generated files.
hl.window_rule({
    match = { class = "^aurora-studio$" },
    float = true,
    center = true,
    no_blur = false,
    suppress_event = "maximize",
})
