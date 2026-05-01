local SpinAndBob = {}

function SpinAndBob:OnCreate()
    self.rotation_speed_degrees = self.rotation_speed_degrees or 90.0
    self.bob_amplitude = self.bob_amplitude or 0.35
    self.bob_frequency = self.bob_frequency or 1.5

    self._origin_y = self.entity.translation.y
    self._base_rotation_y = self.entity.rotation.y

    Log.info("SpinAndBob created for " .. self.entity.name)
end

function SpinAndBob:OnUpdate(dt)
    local translation = self.entity.translation
    translation.y = self._origin_y + math.sin(Time.elapsed_time * self.bob_frequency) * self.bob_amplitude
    self.entity.translation = translation

    local rotation = self.entity.rotation
    rotation.y = self._base_rotation_y + math.rad(self.rotation_speed_degrees) * Time.elapsed_time
    self.entity.rotation = rotation
end

function SpinAndBob:OnDestroy()
    Log.info("SpinAndBob destroyed for " .. self.entity.name)
end

return SpinAndBob
