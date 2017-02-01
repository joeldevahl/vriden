Unit.config_set = { "optimizations" }

function Unit.Build(self)
	if target.family == "windows" then
		self.settings.cc.defines:Add("_SCL_SECURE_NO_WARNINGS") -- Disable some warnings
		self.settings.cc.defines:Add("_CRT_NONSTDC_NO_DEPRECATE") -- Disable some warnings
	end
	DefaultBuild(self)
end
