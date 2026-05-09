local PlayerController = {}

PlayerController.Properties = {
    enabled = {
        type = "Bool",
        default = true,
        display_name = "Enabled",
        description = "Controls whether this script updates the player."
    },
    camera = {
        type = "Entity",
        display_name = "Camera",
        description = "Child camera entity driven by pitch."
    },
    require_mouse_button = {
        type = "Bool",
        default = true,
        display_name = "Require Mouse Button",
        description = "Only captures look input while the capture button is pressed."
    },
    capture_button = {
        type = "Int",
        default = MouseCode.Right,
        display_name = "Capture Button",
        description = "Mouse button used to capture first-person input."
    },
    move_speed = {
        type = "Float",
        default = 5.0,
        display_name = "Move Speed",
        description = "Horizontal movement speed in world units per second."
    },
    sprint_multiplier = {
        type = "Float",
        default = 2.0,
        display_name = "Sprint Multiplier",
        description = "Movement multiplier while Left Shift is held."
    },
    mouse_sensitivity = {
        type = "Float",
        default = 0.0025,
        display_name = "Mouse Sensitivity",
        description = "Radians of camera rotation applied per mouse pixel."
    },
    max_mouse_delta = {
        type = "Float",
        default = 240.0,
        display_name = "Max Mouse Delta",
        description = "Maximum mouse delta consumed in one frame."
    },
    min_pitch = {
        type = "Float",
        default = -1.553343,
        display_name = "Min Pitch",
        description = "Minimum camera pitch in radians."
    },
    max_pitch = {
        type = "Float",
        default = 1.553343,
        display_name = "Max Pitch",
        description = "Maximum camera pitch in radians."
    }
}

local function clamp(value, min_value, max_value)
    return math.max(min_value, math.min(max_value, value))
end

local function wrap_angle(value)
    local two_pi = math.pi * 2.0
    value = (value + math.pi) % two_pi
    if value < 0.0 then
        value = value + two_pi
    end
    return value - math.pi
end

local function normalized_axis(x, z)
    local length = math.sqrt(x * x + z * z)
    if length <= 0.00001 then
        return 0.0, 0.0
    end

    return x / length, z / length
end

function PlayerController:_camera()
    if self.camera ~= nil and self.camera:is_valid() then
        return self.camera
    end

    return self.entity
end

function PlayerController:_sync_rotation()
    local camera = self:_camera()
    if camera.uuid == self.entity.uuid then
        self.entity.rotation = Vec3(self._pitch, self._yaw, 0.0)
        return
    end

    self.entity.rotation = Vec3(0.0, self._yaw, 0.0)
    camera.rotation = Vec3(self._pitch, 0.0, 0.0)
end

function PlayerController:_begin_capture()
    if self._capturing then
        return
    end

    Input.set_cursor_mode(CursorMode.Locked)
    Input.set_raw_mouse_motion(true)
    self._capturing = true
    self._ignore_next_mouse_delta = true
end

function PlayerController:_end_capture()
    if not self._capturing then
        return
    end

    Input.set_cursor_mode(CursorMode.Normal)
    Input.set_raw_mouse_motion(false)
    self._capturing = false
end

function PlayerController:OnCreate()
    self.enabled = self.enabled ~= false
    self.require_mouse_button = self.require_mouse_button ~= false
    self.capture_button = self.capture_button or MouseCode.Right
    self.move_speed = self.move_speed or 5.0
    self.sprint_multiplier = self.sprint_multiplier or 2.0
    self.mouse_sensitivity = self.mouse_sensitivity or 0.0025
    self.max_mouse_delta = self.max_mouse_delta or 240.0
    self.min_pitch = self.min_pitch or -1.553343
    self.max_pitch = self.max_pitch or 1.553343

    local player_rotation = self.entity.rotation
    local camera_rotation = self:_camera().rotation
    self._yaw = player_rotation.y
    self._pitch = camera_rotation.x
    self._capturing = false
    self._ignore_next_mouse_delta = false

    local camera = self:_camera()
    if camera:has_camera() then
        camera:set_primary_camera(true)
    end
    self:_sync_rotation()
end

function PlayerController:OnUpdate(dt)
    if not self.enabled then
        self:_end_capture()
        return
    end

    if Input.is_key_pressed(KeyCode.Escape) then
        self:_end_capture()
        return
    end

    if self.require_mouse_button and not Input.is_mouse_button_pressed(self.capture_button) then
        self:_end_capture()
        return
    end

    self:_begin_capture()

    local mouse_dx, mouse_dy = Input.get_mouse_delta()
    if self._ignore_next_mouse_delta then
        mouse_dx = 0.0
        mouse_dy = 0.0
        self._ignore_next_mouse_delta = false
    end

    mouse_dx = clamp(mouse_dx, -self.max_mouse_delta, self.max_mouse_delta)
    mouse_dy = clamp(mouse_dy, -self.max_mouse_delta, self.max_mouse_delta)

    self._yaw = wrap_angle(self._yaw - mouse_dx * self.mouse_sensitivity)
    self._pitch = clamp(self._pitch + mouse_dy * self.mouse_sensitivity, self.min_pitch, self.max_pitch)
    self:_sync_rotation()

    local x = 0.0
    local z = 0.0
    if Input.is_key_pressed(KeyCode.W) then z = z + 1.0 end
    if Input.is_key_pressed(KeyCode.S) then z = z - 1.0 end
    if Input.is_key_pressed(KeyCode.D) then x = x + 1.0 end
    if Input.is_key_pressed(KeyCode.A) then x = x - 1.0 end

    x, z = normalized_axis(x, z)
    if x == 0.0 and z == 0.0 then
        return
    end

    local speed = self.move_speed
    if Input.is_key_pressed(KeyCode.LeftShift) then
        speed = speed * self.sprint_multiplier
    end

    local distance = speed * dt
    local yaw = self._yaw
    local world_x = (math.cos(yaw) * x - math.sin(yaw) * z) * distance
    local world_z = (-math.sin(yaw) * x - math.cos(yaw) * z) * distance
    self.entity:translate_world(Vec3(world_x, 0.0, world_z))
end

function PlayerController:OnDestroy()
    self:_end_capture()
end

return PlayerController
