local SchemaDrivenMover = {}

SchemaDrivenMover.Properties = {
    enabled = {
        type = "Bool",
        default = true,
        display_name = "Enabled",
        description = "Controls whether this script updates the entity."
    },
    speed = {
        type = "Float",
        default = 1.0,
        display_name = "Speed",
        description = "Horizontal movement speed."
    },
    amplitude = {
        type = "Float",
        default = 0.5,
        display_name = "Amplitude",
        description = "Vertical bob amplitude."
    },
    frequency = {
        type = "Float",
        default = 1.5,
        display_name = "Frequency",
        description = "Vertical bob frequency."
    },
    rotation_speed = {
        type = "Float",
        default = 45.0,
        display_name = "Rotation Speed",
        description = "Rotation speed in degrees per second."
    },
    offset = {
        type = "Vec3",
        default = { x = 0.0, y = 0.0, z = 0.0 },
        display_name = "Offset",
        description = "Additional world-space offset from the starting position."
    },
    label = {
        type = "String",
        default = "SchemaDrivenMover",
        display_name = "Label",
        description = "Text printed when the script starts."
    },
    target = {
        type = "Entity",
        display_name = "Target",
        description = "Optional entity reference used to verify Entity property injection."
    },
    preview_asset = {
        type = "Asset",
        display_name = "Preview Asset",
        description = "Optional asset reference used to verify Asset property injection."
    }
}

local function vec3_or_zero(value)
    if value == nil then
        return Vec3(0.0, 0.0, 0.0)
    end
    return value
end

function SchemaDrivenMover:OnCreate()
    self.enabled = self.enabled ~= false
    self.speed = self.speed or 1.0
    self.amplitude = self.amplitude or 0.5
    self.frequency = self.frequency or 1.5
    self.rotation_speed = self.rotation_speed or 45.0
    self.offset = vec3_or_zero(self.offset)
    self.label = self.label or "SchemaDrivenMover"

    self._origin = self.entity.translation
    self._origin_rotation = self.entity.rotation
    self._time = 0.0

    local target_name = "none"
    if self.target ~= nil and self.target:is_valid() then
        target_name = self.target.name
    end

    Log.info(self.label .. " started on " .. self.entity.name .. ", target=" .. target_name .. ", asset=" .. tostring(self.preview_asset or 0))
end

function SchemaDrivenMover:OnUpdate(dt)
    if not self.enabled then
        return
    end

    self._time = self._time + dt

    local translation = self.entity.translation
    translation.x = self._origin.x + self.offset.x + math.sin(self._time * self.speed) * 0.75
    translation.y = self._origin.y + self.offset.y + math.sin(self._time * self.frequency) * self.amplitude
    translation.z = self._origin.z + self.offset.z
    self.entity.translation = translation

    local rotation = self.entity.rotation
    rotation.y = self._origin_rotation.y + math.rad(self.rotation_speed) * self._time
    self.entity.rotation = rotation
end

function SchemaDrivenMover:OnDestroy()
    Log.info(self.label .. " stopped on " .. self.entity.name)
end

return SchemaDrivenMover
